#include <hive/chain/block_log_wrapper.hpp>

#include <hive/chain/block_log.hpp>
#include <hive/utilities/split_block_log.hpp>
//#include <regex>

namespace hive { namespace chain {

/*static*/ std::shared_ptr< block_log_wrapper > block_log_wrapper::create_wrapper( int block_log_split,
  appbase::application& app, blockchain_worker_thread_pool& thread_pool )
{
  // Allowed values are -1, 0 and positive.
  if( block_log_split < LEGACY_SINGLE_FILE_BLOCK_LOG )
  {
    FC_THROW_EXCEPTION( fc::parse_error_exception, "Not supported block log split value" );
  }

  return std::make_shared< block_log_wrapper >( block_log_split, app, thread_pool );
}

/*static*/ std::shared_ptr< block_log_wrapper > block_log_wrapper::create_opened_wrapper(
  const fc::path& the_path, appbase::application& app,
  blockchain_worker_thread_pool& thread_pool, bool read_only )
{
  FC_ASSERT( not fc::exists( the_path ) || fc::is_regular_file( the_path ),
    "Path ${p} does NOT point to regular file.", ("p", the_path) );

  if( the_path.filename().string() == block_log_file_name_info::_legacy_file_name )
  {
    auto writer = std::make_shared< block_log_wrapper >( LEGACY_SINGLE_FILE_BLOCK_LOG, app, thread_pool );
    writer->open_and_init( the_path, read_only );
    return writer;
  }

  uint32_t part_number = block_log_file_name_info::is_part_file( the_path );
  if( part_number > 0 )
  {
    FC_ASSERT( part_number == 1,
              "Expected 1st part file name, not following one (${path})", ("path", the_path) );
    auto writer = std::make_shared< block_log_wrapper >( MULTIPLE_FILES_FULL_BLOCK_LOG, app, thread_pool );
    writer->open_and_init( the_path, read_only, 1/*start_from_part*/ );
    return writer;
  }

  FC_THROW_EXCEPTION( fc::parse_error_exception, 
    "Provided block log path ${path} matches neither legacy single file name nor split log part file name pattern.",
    ("path", the_path) );
}

/*static*/ std::shared_ptr< block_log_wrapper > block_log_wrapper::create_limited_wrapper(
  const fc::path& dir, appbase::application& app, blockchain_worker_thread_pool& thread_pool,
  uint32_t start_from_part /*= 1*/ )
{
  FC_ASSERT( start_from_part > 0 );
  FC_ASSERT( fc::exists( dir ) && fc::is_directory( dir ),
    "Path ${p} is NOT an existing directory.", ("p", dir) );

  auto writer = std::make_shared< block_log_wrapper >( MULTIPLE_FILES_FULL_BLOCK_LOG, app, thread_pool );
  writer->_open_args.data_dir = dir;
  writer->skip_first_parts( start_from_part -1 );

  return writer;
}

/*static*/ uint32_t block_log_wrapper::determine_max_blocks_in_log_file( int block_log_split )
{
  return block_log_split == LEGACY_SINGLE_FILE_BLOCK_LOG ?
          std::numeric_limits<uint32_t>::max() :
          BLOCKS_IN_SPLIT_BLOCK_LOG_FILE;
}

block_log_wrapper::block_log_wrapper( int block_log_split, appbase::application& app,
                                      blockchain_worker_thread_pool& thread_pool )
  : _app( app ), _thread_pool( thread_pool ), 
    _max_blocks_in_log_file( determine_max_blocks_in_log_file( block_log_split ) ),
    _block_log_split( block_log_split )
{}

void block_log_wrapper::open_and_init( const block_log_open_args& bl_open_args, bool read_only )
{
  _open_args = bl_open_args;
  common_open_and_init( read_only, true /*allow_splitting_monolithic_log*/);
}

void block_log_wrapper::open_and_init( const fc::path& path, bool read_only, 
  uint32_t start_from_part /*= 1*/ )
{
  _open_args.data_dir = path.parent_path();
  common_open_and_init( read_only, false /*allow_splitting_monolithic_log*/, start_from_part );
}

void block_log_wrapper::reopen_for_writing()
{
  // Only head part file is ever opened for writing.
  ilog("Reopening block log for writing.");
  auto head_log_ptr = get_head_log();
  internal_open_and_init( head_log_ptr, head_log_ptr->get_log_file(), false/*read_only*/ );
}

void block_log_wrapper::close_log()
{
  // Make one last attempt to clear GC contents.
  dispose_garbage( true/*closing_time*/ );

  // Simply close remaining logs
  for( auto it = _logs.begin(); it != _logs.end(); ++it )
  {
    block_log_ptr_t& log_ref = *it;
    if( log_ref )
    { 
      log_ref->close();
      log_ref.reset();
    }
  }  
}

void block_log_wrapper::dispose_garbage( bool closing_time )
{
  for( auto it = _garbage_collection.begin(); it != _garbage_collection.end(); ++it )
  {
    if( not (*it) )
      continue;

    block_log_ptr_t& log_ref = *it; 

    FC_ASSERT( not closing_time || it->use_count() == 1,
      "Failed to close block log ${file}. Most likely some API thread is still using it.",
      ("file", log_ref->get_log_file()) );
    
    // Is the log ready for closing?
    if( it->use_count() != 1 )
      continue;

    fc::path log_file = log_ref->get_log_file();
    fc::path artifacts_file = log_ref->get_artifacts_file();

    log_ref->close();
    log_ref.reset();

    fc::remove( log_file );
    fc::remove( artifacts_file );
  }
}

void block_log_wrapper::skip_first_parts( uint32_t parts_to_skip )
{
  while( parts_to_skip > 0 )
  {
    _logs.push_back( block_log_ptr_t() );
    --parts_to_skip;
  }
}

std::shared_ptr< block_log > block_log_wrapper::get_head_log() const
{
  return std::atomic_load( &( _logs.back() ) );
}

std::tuple<std::unique_ptr<char[]>, size_t, block_attributes_t> block_log_wrapper::read_raw_head_block() const
{
  return get_head_log()->read_raw_head_block();
}

std::tuple<std::unique_ptr<char[]>, size_t, block_log_artifacts::artifacts_t> block_log_wrapper::read_raw_block_data_by_num(uint32_t block_num) const
{
  const block_log_ptr_t log = get_block_log_corresponding_to( block_num );
  FC_ASSERT( log, 
             "Unable to find block log corresponding to block number ${block_num}", (block_num));
  return log->read_raw_block_data_by_num( block_num );
}

void block_log_wrapper::append( const std::shared_ptr<full_block_type>& full_block, const bool is_at_live_sync )
{
  internal_append( full_block->get_block_num(), [&]( block_log_ptr_t log ){ 
    log->append( full_block, is_at_live_sync );
  });
}

uint64_t block_log_wrapper::append_raw( uint32_t block_num, const char* raw_block_data,
  size_t raw_block_size, const block_attributes_t& flags, const bool is_at_live_sync )
{
  uint64_t result = 0;
  internal_append( block_num, [&]( block_log_ptr_t log ){ 
    result = log->append_raw( block_num, raw_block_data, raw_block_size, flags, is_at_live_sync );
  });
  return result;
}

void block_log_wrapper::flush_head_log()
{
  get_head_log()->flush();
}

std::shared_ptr<full_block_type> block_log_wrapper::head_block() const
{
  return get_head_block();
}

uint32_t block_log_wrapper::head_block_num( 
  fc::microseconds wait_for_microseconds /*= fc::microseconds()*/ ) const
{
  const auto hb = get_head_block();
  return hb ? hb->get_block_num() : 0;
}

block_id_type block_log_wrapper::head_block_id( 
  fc::microseconds wait_for_microseconds /*= fc::microseconds()*/ ) const
{
  const auto hb = get_head_block();
  return hb ? hb->get_block_id() : block_id_type();
}

std::shared_ptr<full_block_type> block_log_wrapper::read_block_by_num( uint32_t block_num ) const
{
  const block_log_ptr_t log = get_block_log_corresponding_to( block_num );
  return log ? log->read_block_by_num( block_num ) : std::shared_ptr<full_block_type>();
}

void block_log_wrapper::process_blocks(uint32_t starting_block_number,
  uint32_t ending_block_number, block_processor_t processor,
  hive::chain::blockchain_worker_thread_pool& thread_pool) const
{
  block_log_ptr_t current_log;
  const block_log_ptr_t head_log = get_head_log();
  do
  {
    current_log = get_block_log_corresponding_to( starting_block_number );
    if( not current_log )
      return;

    uint32_t last_block_of_part = 
      get_part_number_for_block( starting_block_number, _max_blocks_in_log_file ) *
      _max_blocks_in_log_file;
    current_log->for_each_block(  starting_block_number, std::min( last_block_of_part, ending_block_number ),
                                  processor, block_log::for_each_purpose::replay, thread_pool );
    
    starting_block_number = last_block_of_part + 1;
  }
  while( starting_block_number < ending_block_number && current_log != head_log );
}

std::shared_ptr<full_block_type> block_log_wrapper::fetch_block_by_id( 
  const block_id_type& id ) const
{
  try {
    std::shared_ptr<full_block_type> block = 
      read_block_by_num( protocol::block_header::num_from_id( id ) );
    if( block && block->get_block_id() == id )
      return block;
    return std::shared_ptr<full_block_type>();
  } FC_CAPTURE_AND_RETHROW()
}

std::shared_ptr<full_block_type> block_log_wrapper::get_block_by_number( uint32_t block_num,
  fc::microseconds wait_for_microseconds ) const
{
  // For the time being we'll silently return empty pointer for requests of future blocks.
  // FC_ASSERT( block_num <= head_block_num(),
  //           "Got no block with number greater than ${num}.", ("num", head_block_num()) );
  if( block_num > head_block_num() )
    return std::shared_ptr<full_block_type>();

  // Legacy behavior.
  if( block_num == 0 )
    return std::shared_ptr<full_block_type>();

  const block_log_ptr_t log = get_block_log_corresponding_to( block_num );
  if( _block_log_split > MULTIPLE_FILES_FULL_BLOCK_LOG )
  {
    FC_ASSERT( log,
      "Block ${num} has been pruned (oldest stored block is ${old}). "
      "Consider disabling pruning or increasing block-log-split value (currently ${part_count}).",
      ("num", block_num)("old", get_tail_block_num())("part_count", _block_log_split) );
  }
  else
  {
    FC_ASSERT( log,
      "Internal error, block ${block_num} should have been found in block log file", (block_num) );
  }

  return log->read_block_by_num( block_num );
}

std::vector<std::shared_ptr<full_block_type>> block_log_wrapper::fetch_block_range( 
  const uint32_t starting_block_num, const uint32_t count, 
  fc::microseconds wait_for_microseconds /*= fc::microseconds()*/ ) const
{ 
  try {
    FC_ASSERT(starting_block_num > 0, "Invalid starting block number");
    FC_ASSERT(count > 0, "Why ask for zero blocks?");
    FC_ASSERT(count <= 1000, "You can only ask for 1000 blocks at a time");
    idump((starting_block_num)(count));

    std::vector<std::shared_ptr<full_block_type>> result;
    result = read_block_range_by_num(starting_block_num, count);

    idump((result.size()));
    if (!result.empty())
      idump((result.front()->get_block_num())(result.back()->get_block_num()));

    return result;
  } FC_LOG_AND_RETHROW()
}

bool block_log_wrapper::is_known_block(const block_id_type& id) const
{
  try {
    auto requested_block_num = protocol::block_header::num_from_id(id);
    block_id_type read_block_id = read_block_id_by_num( requested_block_num );
    return read_block_id != block_id_type() && read_block_id == id;
  } FC_CAPTURE_AND_RETHROW()
}

std::deque<block_id_type>::const_iterator block_log_wrapper::find_first_item_not_in_blockchain(
  const std::deque<block_id_type>& item_hashes_received ) const
{
  return std::partition_point(item_hashes_received.begin(), item_hashes_received.end(), [&](const block_id_type& block_id) {
    return is_known_block(block_id);
  });
}

block_id_type block_log_wrapper::find_block_id_for_num( uint32_t block_num ) const
{
  block_id_type result;

  try
  {
    if( block_num != 0 )
    {
      result = read_block_id_by_num( block_num );
    }
  }
  FC_CAPTURE_AND_RETHROW( (block_num) )

  if( result == block_id_type() )
    FC_THROW_EXCEPTION(fc::key_not_found_exception, "block number not found");

  return result;
}

std::vector<block_id_type> block_log_wrapper::get_blockchain_synopsis( 
  const block_id_type& reference_point, uint32_t number_of_blocks_after_reference_point ) const
{
  //std::vector<block_id_type> synopsis = _fork_db.get_blockchain_synopsis(reference_point, number_of_blocks_after_reference_point, block_number_needed_from_block_log);
  std::vector<block_id_type> synopsis;
  uint32_t last_irreversible_block_num = head_block()->get_block_num();
  uint32_t reference_point_block_num = protocol::block_header::num_from_id(reference_point);
  if (reference_point_block_num < last_irreversible_block_num)
  {
    uint32_t block_number_needed_from_block_log = reference_point_block_num;
    uint32_t reference_point_block_num = protocol::block_header::num_from_id(reference_point);
    auto read_block_id = read_block_id_by_num( block_number_needed_from_block_log );

    if (reference_point_block_num == block_number_needed_from_block_log)
    {
      // we're getting this block from the database because it's the reference point,
      // not because it's the last irreversible.
      // We can only do this if the reference point really is in the blockchain
      if (read_block_id == reference_point)
        synopsis.insert(synopsis.begin(), reference_point);
      else
      {
        // TODO: Update the comment below with the possibility of block_number_needed_from_block_log having been pruned.
        wlog("Unable to generate a usable synopsis because the peer we're generating it for forked too long ago "
             "(our chains diverge before block #${reference_point_block_num}",
             (reference_point_block_num));
        // TODO: get the right type of exception here
        //FC_THROW_EXCEPTION(graphene::net::block_older_than_undo_history, "Peer is on a fork I'm unable to switch to");
        FC_THROW("Peer is on a fork I'm unable to switch to");
      }
    }
    else
    {
      synopsis.insert(synopsis.begin(), read_block_id);
    }
  }
  return synopsis;
}

std::vector<block_id_type> block_log_wrapper::get_block_ids(
  const std::vector<block_id_type>& blockchain_synopsis, uint32_t& remaining_item_count,
  uint32_t limit) const
{
  remaining_item_count = 0;
  return vector<block_id_type>();
}

const std::shared_ptr<full_block_type> block_log_wrapper::get_head_block() const
{
  return get_head_log()->head();
}

block_id_type block_log_wrapper::read_block_id_by_num( uint32_t block_num ) const
{
  const block_log_ptr_t log = get_block_log_corresponding_to( block_num );
  return log ? log->read_block_id_by_num( block_num ) : block_id_type();
}

const block_log_wrapper::block_log_ptr_t block_log_wrapper::get_block_log_corresponding_to(
  uint32_t block_num ) const
{
  uint32_t request_part_number = get_part_number_for_block( block_num, _max_blocks_in_log_file );
  if( request_part_number > _logs.size() )
    return block_log_ptr_t();

  return std::atomic_load( &(_logs[ request_part_number-1 ]) );
}

block_log_wrapper::full_block_range_t block_log_wrapper::read_block_range_by_num(
  uint32_t starting_block_num, uint32_t count ) const
{
  full_block_range_t result;
  block_log_ptr_t current_log = nullptr;
  while( count > 0 )
  {
    current_log = get_block_log_corresponding_to( starting_block_num );
    if( not current_log )
      return result;

    auto part_result = current_log->read_block_range_by_num( starting_block_num, count );
    size_t part_result_count = part_result.size();
    if( part_result_count == 0 )
      return result;

    starting_block_num += part_result_count;
    count -= part_result_count;
    result.insert( result.end(), part_result.begin(), part_result.end() );
  }

  return result;
}

void block_log_wrapper::internal_open_and_init( block_log_ptr_t the_log, const fc::path& path,
                                                bool read_only )
{
  the_log->open_and_init( path,
                          read_only,
                          _open_args.enable_block_log_compression,
                          _open_args.block_log_compression_level,
                          _open_args.enable_block_log_auto_fixing,
                          _thread_pool );
}

uint32_t block_log_wrapper::validate_tail_part_number( uint32_t tail_part_number, 
  uint32_t head_part_number ) const
{
  FC_ASSERT( _block_log_split > LEGACY_SINGLE_FILE_BLOCK_LOG );

  if( _block_log_split == MULTIPLE_FILES_FULL_BLOCK_LOG )
  {
    // Expected tail part is obviously 1 - we need each part.
    if( tail_part_number > 1 )
      throw std::runtime_error( 
        "Missing block log part file(s), beginning with file #" + std::to_string( tail_part_number-1 ) );

    return 1;
  }

  FC_ASSERT( head_part_number >= tail_part_number );

  // Require configured number of log file parts, unless we're only starting.
  if( tail_part_number > 1 &&
      head_part_number - tail_part_number < (unsigned int)_block_log_split )
    throw std::runtime_error( 
      "Too few block log part files found (" + std::to_string( head_part_number - tail_part_number ) +
      "), " + std::to_string( _block_log_split ) + " required." );

  return head_part_number > (unsigned int)_block_log_split ?
          head_part_number - _block_log_split :
          1;
}

bool block_log_wrapper::try_splitting_monolithic_log_file(
  uint32_t head_part_number /*= 0*/, size_t part_count /*= 0*/  )
{
  fc::path monolith_path( _open_args.data_dir / block_log_file_name_info::_legacy_file_name );
  if( fc::exists( monolith_path ) && fc::is_regular_file( monolith_path ) )
  {
    try
    {
      wlog("Trying to split legacy monolithic block log file.");
      utilities::split_block_log( monolith_path, head_part_number, part_count, _app, _thread_pool );
      wlog("Successfully split legacy monolithic block log file.");
      return true;
    }
    catch( fc::exception& e )
    {
      elog( "Error while trying to split legacy monolithic block log file: ${e}", (e) );
    }
  }
  else
    ilog("Unable to auto-split legacy monolithic log file - not found.");

  return false;
}

uint32_t block_log_wrapper::force_parts_exist( uint32_t head_part_number, 
  part_file_names_t& part_file_names, bool allow_splitting_monolithic_log )
{
  FC_ASSERT( _block_log_split > LEGACY_SINGLE_FILE_BLOCK_LOG );
  uint32_t tail_part_number = part_file_names.cbegin()->part_number;
  FC_ASSERT( head_part_number >= tail_part_number );

  // Determine actual needed tail part number.
  uint32_t actual_tail_needed = 0;
  // Is block log not pruned or effectively not pruned (yet)?
  if( _block_log_split == MULTIPLE_FILES_FULL_BLOCK_LOG ||
      head_part_number <= (unsigned int)_block_log_split )
  {
    // Expected tail part is obviously 1 - we need each part.
    actual_tail_needed = 1;
  }
  else // pruned log here
  {
    // not all parts are needed here, i.e. head_part_number > _block_log_split
    actual_tail_needed = head_part_number - _block_log_split;
  }

  // Walk over existing parts checking numbers continuity.
  uint32_t last_continuous_number = head_part_number;
  uint32_t last_checked_number = head_part_number;
  for( auto crit = ++( part_file_names.crbegin() ); crit != part_file_names.crend(); ++crit )
  {
    last_checked_number = crit->part_number;
    if( last_checked_number != last_continuous_number -1 )
      break; // Missing parts detected.

    --last_continuous_number;
  }

  // Any missing needed part files?
  if( tail_part_number > actual_tail_needed ||      // There's too few parts or
      last_continuous_number > actual_tail_needed ) // a part file is missing in the middle and we need it.
  {
    uint32_t low_missing_part_number = 
      last_checked_number == actual_tail_needed ? actual_tail_needed +1: actual_tail_needed;
    uint32_t high_missing_part_number = last_continuous_number -1;

    // Is there a separated part in the range of first/last missing ones?
    bool blocking_part_exists = 
      last_checked_number < last_continuous_number && // there's a gap
      last_checked_number > low_missing_part_number; // and a separated part is inside missing range

    std::stringstream msg;
    msg << "Missing block log part file(s) in the range [" 
      << block_log_file_name_info::get_nth_part_file_name( low_missing_part_number )
      << ","
      << block_log_file_name_info::get_nth_part_file_name( high_missing_part_number )
      << "].";

    // Examine possibility of generating missing part files.
    if( not allow_splitting_monolithic_log || // splitting not allowed or
        blocking_part_exists )                // there's a part that we don't want to overwrite.
    {
      msg <<
        ( allow_splitting_monolithic_log ? " Existing part file is blocking" : " Not allowed" ) <<
        " automatic splitting of legacy monolithic block log.";
      throw std::runtime_error( msg.str() );
    }

    size_t needed_part_count = high_missing_part_number - low_missing_part_number + 1;
    if( not try_splitting_monolithic_log_file( high_missing_part_number, needed_part_count ) )
    {
      msg <<
        " Failed to generate missing block log part file(s) by splitting legacy monolithic block log file.";
      throw std::runtime_error( msg.str() );
    }

    look_for_part_files( part_file_names );
  }

  // At this point all needed parts are available (or exception would have been thrown).
  return actual_tail_needed;
}

void block_log_wrapper::look_for_part_files( part_file_names_t& part_file_names )
{
  if( not exists( _open_args.data_dir ) )
  {
    create_directories( _open_args.data_dir );
  }
  else // look for existing log parts
  {
    fc::directory_iterator it( _open_args.data_dir );
    fc::directory_iterator end_it;
    for( ; it != end_it; it++ )
    {
      uint32_t part_number = 0;
      if( fc::is_regular_file( *it ) && 
          ( part_number = block_log_file_name_info::is_part_file( *it ) ) > 0 )
      {
        // Part numbers are always positive.
        part_file_names.insert( { part_number, *it } );
      }
    }
  }
}

void block_log_wrapper::common_open_and_init( bool read_only, bool allow_splitting_monolithic_log, 
  uint32_t start_from_part /*= 1*/ )
{
  FC_ASSERT( start_from_part > 0 );

  if( _block_log_split == LEGACY_SINGLE_FILE_BLOCK_LOG )
  {
    auto single_part_log = std::make_shared<block_log>( _app );
    internal_open_and_init( single_part_log, 
                            _open_args.data_dir / block_log_file_name_info::_legacy_file_name,
                            read_only );
    _logs.push_back( single_part_log );
    return;
  }

  // Any log file created on previous run?
  part_file_names_t part_file_names;
  look_for_part_files( part_file_names );
  
  if( part_file_names.empty() )
  {
    // No part file name found. Try splitting legacy monolithic file if allowed & possible.
    size_t needed_part_count = 
      _block_log_split == MULTIPLE_FILES_FULL_BLOCK_LOG ? 0 /*all*/ : _block_log_split +1;
    if( allow_splitting_monolithic_log &&
        try_splitting_monolithic_log_file( 0/*determine head part from source*/, needed_part_count ) )
    {
      look_for_part_files( part_file_names );
      allow_splitting_monolithic_log = false;
      // Now we can continue opening split log file(s) as if the log was split from the beginning.
    }
    else
    {
      skip_first_parts( start_from_part -1 );
      // Otherwise create, open & set empty initial part log object.
      uint32_t part_number = start_from_part;
      fc::path part_file_path( _open_args.data_dir / block_log_file_name_info::get_nth_part_file_name( part_number ).c_str() );

      const auto first_part_log = std::make_shared<block_log>( _app );
      internal_open_and_init( first_part_log, part_file_path, read_only );
      _logs.push_back( first_part_log );
      return;
    }
  }

  // Make sure all parts required by configuration are there.
  uint32_t head_part_number = part_file_names.crbegin()->part_number;
  uint32_t actual_tail_number_needed = 
    force_parts_exist( head_part_number, part_file_names, allow_splitting_monolithic_log );

  // Open all needed parts.
  _logs.resize( head_part_number, block_log_ptr_t() );
  for( auto cit = part_file_names.cbegin(); cit != part_file_names.cend(); ++cit )
  {
    uint32_t part_number = cit->part_number;
    if( part_number < actual_tail_number_needed )
      continue;

    const auto nth_part_log = std::make_shared<block_log>( _app );
    // Open all non-head parts for reading only (won't be appended),
    // use provided flag for head part.
    bool open_ro = part_number < head_part_number ? true : read_only;
    internal_open_and_init( nth_part_log, cit->part_file, open_ro );
    // Part numbers are always positive.
    std::atomic_store( &( _logs[ part_number-1 ] ), nth_part_log );
  }
}

void block_log_wrapper::wipe_files( const fc::path& dir )
{
  if( not exists( dir ) )
    return;

  auto remove_artifacts = [&]( const fc::path& block_file ) {
    fc::path artifacts_file( block_file.generic_string() +
                             block_log_file_name_info::_artifacts_extension.c_str() );
    if( exists( artifacts_file ) )
      fc::remove( artifacts_file );
  };

  if( _block_log_split == LEGACY_SINGLE_FILE_BLOCK_LOG )
  {
    fc::path legacy_file( dir / block_log_file_name_info::_legacy_file_name );
    if( exists( legacy_file ) )
      fc::remove( legacy_file );

    remove_artifacts( legacy_file );      
    return;
  }

  fc::directory_iterator it( dir );
  fc::directory_iterator end_it;
  for( ; it != end_it; it++ )
  {
    if( fc::is_regular_file( *it ) && 
        block_log_file_name_info::is_part_file( *it ) > 0 )
    {
      fc::remove( *it );
      remove_artifacts( *it );
    }
  }
}

void block_log_wrapper::internal_append( uint32_t block_num, append_t do_appending)
{
  FC_ASSERT( block_num > 0 );

  // Note that we use provided block_num here instead of checking top log's head, as the latter
  // may not be updated when low-level appending using append_raw.

  uint32_t block_part_number = 0;
  // Is it time to switch to a new file & append there?
  if( is_last_number_of_the_file( block_num -1 ) &&
      ( block_part_number = get_part_number_for_block( block_num, _max_blocks_in_log_file ) ) > _logs.size() )
  {
    fc::path new_path = _open_args.data_dir / block_log_file_name_info::get_nth_part_file_name( block_part_number ).c_str();
    const auto new_part_log = std::make_shared<block_log>( _app );
    internal_open_and_init( new_part_log, new_path, false /*read_only*/ );
    // Top log must keep valid head block. Append first, add on top later.
    do_appending( new_part_log );
    _logs.push_back( new_part_log );
    // Shall we delete old file (rotation)?
    // Note that initial part number is 1, new one must be at least 2.
    if( _block_log_split > 0 &&
        block_part_number -1 > (unsigned int)_block_log_split )
      {
        uint32_t removed_part_number = block_part_number -1 -(unsigned int)_block_log_split; // is > 0
        block_log_ptr_t& log_ref = _logs[ removed_part_number -1 ];
        block_log_ptr_t removed_log = std::atomic_exchange( &log_ref, block_log_ptr_t() );
        // Set log to be closed, destroyed & have its files wiped when it's safe to do so.
        _garbage_collection.push_back( removed_log );
        // Try garbage collection.
        dispose_garbage( false/*closing_time*/ );
      }
  }
  else // Simply append to existing file.
  {
    do_appending( get_head_log() );
  }
}

uint32_t block_log_wrapper::get_tail_block_num() const
{
  if( _block_log_split == LEGACY_SINGLE_FILE_BLOCK_LOG ||
      _block_log_split == MULTIPLE_FILES_FULL_BLOCK_LOG )
  {
    return 1;
  }

  int oldest_available_part = std::max<int>( _logs.size() - _block_log_split, 1 );
  return (oldest_available_part -1) * BLOCKS_IN_SPLIT_BLOCK_LOG_FILE + 1;
}

} } //hive::chain
