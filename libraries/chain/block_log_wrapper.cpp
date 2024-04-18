#include <hive/chain/block_log_wrapper.hpp>

#include <hive/chain/block_log_manager.hpp>
//#include <regex>

namespace hive { namespace chain {

block_log_wrapper::block_log_wrapper( int block_log_split, appbase::application& app,
                                      blockchain_worker_thread_pool& thread_pool )
  : _app( app ), _thread_pool( thread_pool ), 
    _max_blocks_in_log_file( block_log_split == LEGACY_SINGLE_FILE_BLOCK_LOG ?
                              std::numeric_limits<uint32_t>::max() :
                              BLOCKS_IN_SPLIT_BLOCK_LOG_FILE),
    _block_log_split( block_log_split )
{}

const std::shared_ptr<full_block_type> block_log_wrapper::get_head_block() const
{
  return _logs.back()->head();
}

block_id_type block_log_wrapper::read_block_id_by_num( uint32_t block_num ) const
{
  const block_log* log = get_block_log_corresponding_to( block_num );
  return log == nullptr ? block_id_type() : log->read_block_id_by_num( block_num );
}

std::shared_ptr<full_block_type> block_log_wrapper::read_block_by_num( uint32_t block_num ) const
{
  const block_log* log = get_block_log_corresponding_to( block_num );
  return log == nullptr ? std::shared_ptr<full_block_type>() : log->read_block_by_num( block_num );
}

const block_log* block_log_wrapper::get_block_log_corresponding_to( uint32_t block_num ) const
{
  uint32_t request_part_number = get_part_number_for_block( block_num );
  if( request_part_number > _logs.size() )
    return nullptr;

  return _logs[ request_part_number-1 ];
}

block_log_reader_common::block_range_t block_log_wrapper::read_block_range_by_num(
  uint32_t starting_block_num, uint32_t count ) const
{
  block_range_t result;
  const block_log* current_log = nullptr;
  while( count > 0 )
  {
    current_log = get_block_log_corresponding_to( starting_block_num );
    if( current_log == nullptr )
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

void block_log_wrapper::internal_open_and_init( block_log* the_log, const fc::path& path,
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

void block_log_wrapper::rotate_part_files( uint32_t new_part_number )
{
  FC_ASSERT( new_part_number > 1 ); // Initial part number is 1, new one must be at least 2.
  FC_ASSERT( _block_log_split > 0 ); // Otherwise no rotation needed.

  if( new_part_number -1 > (unsigned int)_block_log_split )
  {
    uint32_t removed_part_number = new_part_number -1 -(unsigned int)_block_log_split; // is > 0
    block_log* removed_log = _logs[ removed_part_number -1 ];
    _logs[ removed_part_number -1 ] = nullptr;
    FC_ASSERT( removed_log != nullptr );
    fc::path log_file = removed_log->get_log_file();
    fc::path artifacts_file = removed_log->get_artifacts_file();
    removed_log->close();
    delete removed_log;
    fc::remove( log_file );
    fc::remove( artifacts_file );
  }
}

void block_log_wrapper::common_open_and_init( std::optional< bool > read_only )
{
  if( _block_log_split == LEGACY_SINGLE_FILE_BLOCK_LOG )
  {
    block_log* single_part_log = new block_log( _app );
    internal_open_and_init( single_part_log, 
                            _open_args.data_dir / block_log_file_name_info::_legacy_file_name,
                            read_only ? *read_only : false );
    _logs.push_back( single_part_log );
    return;
  }

  // Any log file created on previous run?
  struct part_file_info_t {
    uint32_t part_number = 0;
    fc::path part_file;

    bool operator<(const part_file_info_t& o) const { return part_number < o.part_number; }
  };
  std::set< part_file_info_t > part_file_names;
  if( exists( _open_args.data_dir ) )
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
  
  if( part_file_names.empty() )
  {
    // No part file name found. Create, open & set initial one.
    uint32_t part_number = get_part_number_for_block( 0 );
    fc::path part_file_path( _open_args.data_dir / block_log_file_name_info::get_nth_part_file_name( part_number ).c_str() );

    block_log* first_part_log = new block_log( _app );
    internal_open_and_init( first_part_log, part_file_path, read_only ? *read_only : false );
    _logs.push_back( first_part_log );
    return;
  }

  // Check integrity of found part file names.
  uint32_t head_part_number = part_file_names.crbegin()->part_number;
  uint32_t tail_part_number = part_file_names.cbegin()->part_number;
  // Implementation-dependent check:
  uint32_t actual_tail_needed = validate_tail_part_number( tail_part_number, head_part_number );
  // Shrink the names pool if needed.
  if( actual_tail_needed > tail_part_number )
  {
    auto it = part_file_names.begin();
    std::advance( it, actual_tail_needed - tail_part_number );
    part_file_names.erase( part_file_names.begin(), it );
  }
  // Common check:
  auto crit = part_file_names.crbegin();
  for( uint32_t prev_number = head_part_number +1; crit != part_file_names.crend(); ++crit )
  {
    if( crit->part_number != prev_number -1 )
    {
      throw std::runtime_error( "Broken integrity of block log files: missing file #" + std::to_string( prev_number-1 ) );
    }
    --prev_number;
  }

  // Open them all.
  _logs.resize( head_part_number, nullptr );
  for( auto cit = part_file_names.cbegin(); cit != part_file_names.cend(); ++cit )
  {
    block_log* nth_part_log = new block_log( _app );
    uint32_t part_number = cit->part_number;
    internal_open_and_init( nth_part_log, cit->part_file, read_only ? *read_only : false );
    // Part numbers are always positive.
    _logs[ part_number-1 ] = nth_part_log;
  }
}

void block_log_wrapper::open_and_init( const block_log_open_args& bl_open_args )
{
  _open_args = bl_open_args;
  common_open_and_init( std::optional< bool >() /*set read_only where feasible*/);
}

void block_log_wrapper::open_and_init( const fc::path& path, bool read_only )
{
  _open_args.data_dir = path.parent_path();
  common_open_and_init( std::optional< bool >( read_only ) );
}

void block_log_wrapper::close_log()
{
  std::for_each( _logs.begin(), _logs.end(), [&]( block_log* log ){ 
    if( log )
    { 
      log->close();
      delete log;
    }
  } );
  _logs.clear();
}

std::tuple<std::unique_ptr<char[]>, size_t, block_attributes_t> block_log_wrapper::read_raw_head_block() const
{
  return _logs.back()->read_raw_head_block();
}

std::tuple<std::unique_ptr<char[]>, size_t, block_log_artifacts::artifacts_t> block_log_wrapper::read_raw_block_data_by_num(uint32_t block_num) const
{
  const block_log* log = get_block_log_corresponding_to( block_num );
  FC_ASSERT( log != nullptr, 
             "Unable to find block log corresponding to block number ${block_num}", (block_num));
  return log->read_raw_block_data_by_num( block_num );
}

void block_log_wrapper::internal_append( uint32_t block_num, append_t do_appending)
{
  FC_ASSERT( block_num > 0 );

  // Note that we use provided block_num here instead of checking top log's head, as the latter
  // may not be updated when low-level appending using append_raw.

  // Is it time to switch to a new file?
  if( is_last_number_of_the_file( block_num -1 ) )
  {
    uint32_t new_part_number = get_part_number_for_block( block_num );
    fc::path new_path = _open_args.data_dir / block_log_file_name_info::get_nth_part_file_name( new_part_number ).c_str();
    block_log* new_part_log = new block_log( _app );
    internal_open_and_init( new_part_log, new_path, false /*read_only*/ );
    // Top log must keep valid head block. Append first, add on top later.
    do_appending( new_part_log );
    _logs.push_back( new_part_log );
    if( _block_log_split > 0 )
      rotate_part_files( new_part_number );
  }
  else
  {
    do_appending( _logs.back() );
  }
}

void block_log_wrapper::append( const std::shared_ptr<full_block_type>& full_block, const bool is_at_live_sync )
{
  internal_append( full_block->get_block_num(), [&]( block_log* log ){ 
    log->append( full_block, is_at_live_sync );
  });
}

void block_log_wrapper::flush_head_log()
{
  _logs.back()->flush();
}

uint64_t block_log_wrapper::append_raw( uint32_t block_num, const char* raw_block_data,
  size_t raw_block_size, const block_attributes_t& flags, const bool is_at_live_sync )
{
  uint64_t result = 0;
  internal_append( block_num, [&]( block_log* log ){ 
    result = log->append_raw( block_num, raw_block_data, raw_block_size, flags, is_at_live_sync );
  });
  return result;
}

void block_log_wrapper::process_blocks(uint32_t starting_block_number,
  uint32_t ending_block_number, block_processor_t processor,
  hive::chain::blockchain_worker_thread_pool& thread_pool) const
{
  const block_log* current_log = nullptr;
  const block_log* head_log = _logs.back();
  do
  {
    current_log = get_block_log_corresponding_to( starting_block_number );
    if( current_log == nullptr )
      return;

    uint32_t last_block_of_part = get_part_number_for_block( starting_block_number ) * _max_blocks_in_log_file;
    current_log->for_each_block(  starting_block_number, std::min( last_block_of_part, ending_block_number ),
                                  processor, block_log::for_each_purpose::replay, thread_pool );
    
    starting_block_number = last_block_of_part + 1;
  }
  while( starting_block_number < ending_block_number && current_log != head_log );
}

} } //hive::chain
