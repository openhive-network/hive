
#include <hive/chain/util/advanced_benchmark_dumper.hpp>
#include <chrono>

namespace hive { namespace chain { namespace util {

  uint32_t advanced_benchmark_dumper::cnt = 0;
  std::string advanced_benchmark_dumper::virtual_operation_name = "virtual_operation";
  std::string advanced_benchmark_dumper::apply_context_name = "";

  advanced_benchmark_dumper::advanced_benchmark_dumper()
  {
    if( cnt == 0 )
      file_name = "advanced_benchmark.json";
    else
      file_name = std::to_string( cnt ) + "_advanced_benchmark.json";

    ++cnt;
  }

  advanced_benchmark_dumper::~advanced_benchmark_dumper()
  {
    dump();
  }

  void advanced_benchmark_dumper::begin()
  {
    time_begin = std::chrono::duration_cast<std::chrono::nanoseconds>( std::chrono::system_clock::now().time_since_epoch() ).count();
  }

  void advanced_benchmark_dumper::end( const std::string& context, const std::string& str, uint64_t _count )
  {
    uint64_t time = std::chrono::duration_cast<std::chrono::nanoseconds>(
      std::chrono::system_clock::now().time_since_epoch() ).count() - time_begin;
    bool broken = ( time_begin == 0 );
    auto res = info.emplace( context, str, time, _count, broken );

    if( !res.second )
      res.first->inc( time, _count, broken );
    if( broken )
      return;

    info.inc( time );

    ++flush_cnt;
    if( flush_cnt >= flush_max )
    {
      flush_cnt = 0;
      dump();
    }

    time_begin = 0;
  }

  template< typename COLLECTION >
  void advanced_benchmark_dumper::dump_impl( const total_info< COLLECTION >& src, const std::string& src_file_name )
  {
    const fc::path path( src_file_name.c_str() );
    try
    {
      fc::json::save_to_file( src, path );
    }
    catch ( const fc::exception& except )
    {
      elog( "error writing benchmark data to file ${filename}: ${error}",
          ( "filename", path )("error", except.to_detail_string() ) );
    }
  }

  void advanced_benchmark_dumper::dump()
  {
    total_info< std::multiset< ritem > > rinfo( info.total_time );
    std::for_each(info.items.begin(), info.items.end(), [&rinfo]( const item& obj )
    {
      obj.calculate_time_per_count();
      rinfo.emplace( obj );
    });

    dump_impl( info, file_name );
    dump_impl( rinfo, "r_" + file_name );
  }

} } } // hive::chain::util
