
#include <hive/utilities/benchmark_dumper.hpp>

namespace hive { namespace utilities {

#define PROC_STATUS_LINE_LENGTH 1028

void diff_and_average( benchmark_dumper::counter_t& current, const benchmark_dumper::counter_t& previous )
{
  current.count -= previous.count;
  current.time -= previous.time;
  current.avg_time_ns = current.time / std::max( current.count, 1lu );
}

void diff_and_average( benchmark_dumper::comment_archive_details_t& current, const benchmark_dumper::comment_archive_details_t& previous )
{
  diff_and_average( current.comment_accessed_from_index, previous.comment_accessed_from_index );
  diff_and_average( current.comment_accessed_from_archive, previous.comment_accessed_from_archive );
  diff_and_average( current.comment_not_found, previous.comment_not_found );
  diff_and_average( current.comment_cashout_processing, previous.comment_cashout_processing );
  diff_and_average( current.comment_lib_processing, previous.comment_lib_processing );
}

const benchmark_dumper::measurement& benchmark_dumper::measure( uint32_t block_number, get_stat_details_t get_stat_details )
{
  validate_is_initialized();
  uint64_t current_virtual = 0;
  uint64_t peak_virtual = 0;
  read_mem( _pid, &current_virtual, &peak_virtual );

  fc::time_point current_sys_time = fc::time_point::now();
  clock_t current_cpu_time = clock();

  measurement data;
  uint64_t shm_free = 0;
  get_stat_details( data.index_memory_details_cntr, data.comment_archive_stats, shm_free );
  shm_free /= 1024; // typically db.get_free_memory() so the value is in bytes - recalculate to match other values
  data.set( block_number,
    ( current_sys_time - _last_sys_time ).count() / 1000, // real_ms
    int( ( current_cpu_time - _last_cpu_time ) * 1000 / CLOCKS_PER_SEC ), // cpu_ms
    current_virtual,
    peak_virtual,
    shm_free );
  auto current_comment_archive_stats = data.comment_archive_stats;
  diff_and_average( data.comment_archive_stats, _all_data.total_measurement.comment_archive_stats );

  _last_sys_time = current_sys_time;
  _last_cpu_time = current_cpu_time;
  _total_blocks = block_number;

  _all_data.total_measurement.set( _total_blocks,
    ( _last_sys_time - _init_sys_time ).count() / 1000,
    int( ( _last_cpu_time - _init_cpu_time ) * 1000 / CLOCKS_PER_SEC ),
    current_virtual,
    peak_virtual,
    shm_free );
  _all_data.total_measurement.comment_archive_stats = current_comment_archive_stats;

  // no sense to store data that will not be dumped to file
  if( _all_data.measurements.empty() || is_file_available() )
    _all_data.measurements.push_back( data );
  else
    _all_data.measurements[ 0 ] = data;

  return _all_data.measurements.back();
}

const benchmark_dumper::measurement& benchmark_dumper::dump( bool finalMeasure, uint32_t block_number, get_stat_details_t get_stat_details )
{
  validate_is_initialized();
  if( finalMeasure )
  {
    /// Collect index data including dynamic container sizes, what can be time consuming
    auto& idxData = _all_data.total_measurement.index_memory_details_cntr;
    auto& caData = _all_data.total_measurement.comment_archive_stats;

    idxData.clear();
    uint64_t shm_free = 0;

    get_stat_details( idxData, caData, shm_free );
    _all_data.total_measurement.shm_free = shm_free / 1024;
    _all_data.total_measurement.block_number = block_number;
    diff_and_average( caData, comment_archive_details_t() );

    std::sort( idxData.begin(), idxData.end(),
      []( const index_memory_details_t& info1, const index_memory_details_t& info2 ) -> bool
    {
      return info1.total_index_mem_usage > info2.total_index_mem_usage;
    }
    );
  }

  if( is_file_available() )
  {
    const fc::path path( _file_name );
    try
    {
      fc::json::save_to_file( _all_data, path );
    }
    catch( const fc::exception& except )
    {
      elog( "error writing benchmark data to file ${filename}: ${error}",
        ( "filename", path )( "error", except.to_detail_string() ) );
    }
  }

  return _all_data.total_measurement;
}

typedef std::function<void(const char* key)> TScanErrorCallback;

unsigned long long read_u64_value_from(FILE* input, const char* key, unsigned key_length, const TScanErrorCallback& error_callback)
{
  char line_buffer[PROC_STATUS_LINE_LENGTH];
  while( fgets(line_buffer, PROC_STATUS_LINE_LENGTH, input) != nullptr )
  {
    const char* found_pos = strstr(line_buffer, key);
    if( found_pos != nullptr )
    {
      unsigned long long result = 0;

      if( sscanf(found_pos+key_length, "%llu", &result) != 1 )
      {
        error_callback(key);
      }

      return result;
    }
  }

  error_callback(key);
  return 0;
}

bool benchmark_dumper::read_mem(pid_t pid, uint64_t* current_virtual, uint64_t* peak_virtual)
{
  const char* procPath = "/proc/self/status";
  FILE* input = fopen(procPath, "re");
  if(input == NULL)
  {
    elog( "cannot read: ${file} file.", ("file", procPath) );
    return false;
  }

  TScanErrorCallback error_callback = [procPath](const char* key)
  {
    elog( "cannot read value of ${key} key in ${file}", ("key", key) ("file", procPath) );
  };

  *peak_virtual = read_u64_value_from( input, "VmPeak:", 7, error_callback );
  *current_virtual = read_u64_value_from( input, "VmSize:", 7, error_callback );

  fclose(input);
  return true;
}

} }
