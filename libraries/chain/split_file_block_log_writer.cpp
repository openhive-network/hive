#include <hive/chain/split_file_block_log_writer.hpp>

#include <hive/chain/block_log.hpp>

//#include <regex>

namespace hive { namespace chain {

namespace detail {

  const std::string split_log_part_name_core( "block_log_part" );
  const size_t split_log_part_name_extension_length = 4;

  std::string get_nth_part_file_name( uint32_t part_number )
  {
    size_t padding = split_log_part_name_extension_length;
    std::string part_number_str = std::to_string( part_number );
    std::string result = split_log_part_name_core + "." + 
      std::string(padding - std::min(padding, part_number_str.length()), '0') + part_number_str;
    return result;
  }

  bool is_part_file( const fc::path& file )
  {
    // "^block_log_part\\.\\d{4}$"

    if( file.stem().string() != split_log_part_name_core )
      return false;

    //return std::regex_match( file.extension().string(), std::regex( "^\\d{4}$" ) );
    auto extension = file.extension().string();
    // Note that fc::path::extension() contains leading dot.
    return extension.size() == split_log_part_name_extension_length +1  && 
      std::all_of( ++(extension.begin()), extension.end(), [](unsigned char ch){ return std::isdigit(ch); } );
  }

} //detail

split_file_block_log_writer::split_file_block_log_writer( appbase::application& app, 
  blockchain_worker_thread_pool& thread_pool )
  : _app( app ), _thread_pool( thread_pool )
{}

const block_log& split_file_block_log_writer::get_head_block_log() const
{
  return *( _logs.back() );
}

const block_log* split_file_block_log_writer::get_block_log_corresponding_to( uint32_t block_num ) const
{
  uint32_t request_part_number = get_part_number_for_block( block_num );
  if( request_part_number > _logs.size() )
    return nullptr;

  return _logs[ request_part_number-1 ];
}

block_log_reader_common::block_range_t split_file_block_log_writer::read_block_range_by_num(
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

void split_file_block_log_writer::open_and_init( block_log* the_log, const fc::path& path, bool read_only )
{
  the_log->open_and_init( path,
                          read_only,
                          _open_args.enable_block_log_compression,
                          _open_args.block_log_compression_level,
                          _open_args.enable_block_log_auto_fixing,
                          _thread_pool );
}

void split_file_block_log_writer::open_and_init( const block_log_open_args& bl_open_args )
{
  _open_args = bl_open_args;

  // Any log file created on previous run?
  std::set< std::string > part_file_names;
  if( exists( _open_args.data_dir ) )
  {
    fc::directory_iterator it( _open_args.data_dir );
    fc::directory_iterator end_it;
    for( ; it != end_it; it++ )
    {
      if( fc::is_regular_file( *it ) && detail::is_part_file( *it ) )
      {
        part_file_names.insert( it->filename().string() );
      }
    }
  }
  
  if( part_file_names.empty() )
  {
    // No part file name found. Create, open & set initial one.
    uint32_t part_number = get_part_number_for_block( 0 );
    fc::path part_file_path( _open_args.data_dir / detail::get_nth_part_file_name( part_number ).c_str() );

    block_log* first_part_log = new block_log( _app );
    open_and_init( first_part_log, part_file_path, false /*read_only*/ );
    _logs.push_back( first_part_log );
    return;
  }

  const size_t number_of_parts = part_file_names.size();
  // Check integrity of found part file names.
  for( size_t part_number = 1; part_number <= number_of_parts; ++part_number )
  {
    std::string nth_part_file_name = detail::get_nth_part_file_name( part_number );
    if( part_file_names.find( nth_part_file_name ) == part_file_names.end() )
    {
      throw std::runtime_error( "Broken integrity of block log files: missing file " + nth_part_file_name );
    }
  }

  // Open them all.
  for( size_t part_number = 1; part_number <= number_of_parts; ++part_number )
  {
    std::string nth_part_file_name = detail::get_nth_part_file_name( part_number );
    fc::path nth_part_file_path( _open_args.data_dir / nth_part_file_name.c_str() );
    block_log* nth_part_log = new block_log( _app );
    open_and_init( nth_part_log, nth_part_file_path, part_number < number_of_parts /*read_only*/ );
    _logs.push_back( nth_part_log );
  }
}

void split_file_block_log_writer::close_log()
{
  std::for_each( _logs.begin(), _logs.end(), [&]( block_log* log ){ log->close(); delete log; } );
  _logs.clear();
}

void split_file_block_log_writer::append( const std::shared_ptr<full_block_type>& full_block, const bool is_at_live_sync )
{
  std::shared_ptr<full_block_type> head_block = _logs.back()->head();
  uint32_t head_block_num = head_block == nullptr ? 0 : head_block->get_block_num();

  // Is it time to switch to a new file?
  if( is_last_number_of_the_file( head_block_num ) )
  {
    uint32_t new_part_number = get_part_number_for_block( head_block_num + 1);
    fc::path new_path = _open_args.data_dir / detail::get_nth_part_file_name( new_part_number ).c_str();
    block_log* new_part_log = new block_log( _app );
    open_and_init( new_part_log, new_path, false /*read_only*/ );
    // Top log must keep valid head block. Append first, add on top later.
    new_part_log->append( full_block, is_at_live_sync );
    _logs.push_back( new_part_log );
  }
  else
  {
    _logs.back()->append( full_block, is_at_live_sync );
  }
}

void split_file_block_log_writer::flush_head_log()
{
  return _logs.back()->flush();
}

void split_file_block_log_writer::process_blocks(uint32_t starting_block_number,
  uint32_t ending_block_number, block_processor_t processor,
  hive::chain::blockchain_worker_thread_pool& thread_pool) const
{
  const block_log* current_log = nullptr;
  const block_log& head_log = get_head_block_log();
  do
  {
    current_log = get_block_log_corresponding_to( starting_block_number );
    if( current_log == nullptr )
      return;

    uint32_t last_block_of_part = get_part_number_for_block( starting_block_number ) * BLOCKS_IN_SPLIT_BLOCK_LOG_FILE;
    current_log->for_each_block(  starting_block_number, std::min( last_block_of_part, ending_block_number ),
                                  processor, block_log::for_each_purpose::replay, thread_pool );

    starting_block_number = current_log->head()->get_block_num() + 1;
  }
  while( starting_block_number < ending_block_number && current_log != &head_log );
}

} } //hive::chain
