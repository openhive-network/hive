
#include <hive/utilities/benchmark_dumper.hpp>

namespace hive { namespace utilities {

#define PROC_STATUS_LINE_LENGTH 1028

void operator -= ( benchmark_dumper::counter_t& current, const benchmark_dumper::counter_t& previous )
{
  current.count -= previous.count;
  current.time_ns -= previous.time_ns;
}

void operator -= ( benchmark_dumper::comment_archive_details_t& current, const benchmark_dumper::comment_archive_details_t& previous )
{
  current.comment_accessed_from_index -= previous.comment_accessed_from_index;
  current.comment_accessed_from_archive -= previous.comment_accessed_from_archive;
  current.comment_not_found -= previous.comment_not_found;
  current.comment_cashout_processing -= previous.comment_cashout_processing;
  current.comment_lib_processing -= previous.comment_lib_processing;
}

void benchmark_dumper::initialize( const fc::string& file_name )
{
  _file_name = file_name;
  _last_summary_point.sys_time = fc::time_point::now();
  _last_summary_point.cpu_time = clock();
  _last_measurement_point = _last_summary_point;
  _pid = getpid();

  if( !_file_name.empty() )
  {
    _file.open( _file_name, std::ios::out | std::ios::trunc );
    if( !_file.is_open() )
      elog( "cannot open benchmark data file ${filename}", ( "filename", _file_name ) );
  }
}

void benchmark_dumper::write_to_file( const measurement& data )
{
  if( !_file.is_open() )
    return;

  _file << fc::json::to_string( data ) << '\n';
  _file.flush();
  if( !_file )
    elog( "error writing benchmark data to file ${filename}", ( "filename", _file_name ) );
}

benchmark_dumper::starting_point_t benchmark_dumper::collect( measurement& data, uint32_t block_number,
  const get_stat_details_t& get_stat_details, const starting_point_t& since )
{
  uint64_t current_virtual = 0;
  uint64_t peak_virtual = 0;
  read_mem( _pid, &current_virtual, &peak_virtual );

  starting_point_t now;
  now.sys_time = fc::time_point::now();
  now.cpu_time = clock();

  uint64_t shm_free = 0;
  get_stat_details( data.index_memory_details_cntr, data.comment_archive_stats, shm_free );
  shm_free /= 1024; // typically db.get_free_memory() so the value is in bytes - recalculate to match other values
  data.set( block_number,
    ( now.sys_time - since.sys_time ).count() / 1000, // real_ms
    int( ( now.cpu_time - since.cpu_time ) * 1000 / CLOCKS_PER_SEC ), // cpu_ms
    current_virtual,
    peak_virtual,
    shm_free );
  now.comment_archive_stats = data.comment_archive_stats;
  data.comment_archive_stats -= since.comment_archive_stats;
  return now;
}

const benchmark_dumper::measurement& benchmark_dumper::measure( uint32_t block_number, get_stat_details_t get_stat_details )
{
  validate_is_initialized();
  measurement data;
  _last_measurement_point = collect( data, block_number, get_stat_details, _last_measurement_point );

  _last_measurement = std::move( data );
  write_to_file( _last_measurement );
  return _last_measurement;
}

const benchmark_dumper::measurement& benchmark_dumper::dump( summary_type summary, uint32_t block_number, get_stat_details_t get_stat_details )
{
  validate_is_initialized();
  measurement data;
  data.summary = summary;
  _last_summary_point = _last_measurement_point = collect( data, block_number, get_stat_details, _last_summary_point );

  _last_summary = std::move( data );
  write_to_file( _last_summary );
  return _last_summary;
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
