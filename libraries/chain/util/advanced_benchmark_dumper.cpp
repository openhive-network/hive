
#include <hive/chain/util/advanced_benchmark_dumper.hpp>

#include <fc/exception/exception.hpp>
#include <fc/io/json.hpp>
#include <fc/log/logger.hpp>
#include <fc/reflect/variant.hpp>

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <map>
#include <mutex>
#include <tuple>

namespace hive { namespace chain { namespace util {

namespace {

/// Context and name of all counters registered so far (common to all dumpers)
class counter_registry
{
  public:
    typedef advanced_benchmark_dumper::counter_id_type counter_id_type;
    typedef std::pair< std::string, std::string > key_type; // context, name

    static counter_registry& instance()
    {
      static counter_registry registry;
      return registry;
    }

    counter_id_type register_counter( const std::string& context, const std::string& name )
    {
      std::lock_guard< std::mutex > guard( _mutex );
      auto result = _ids.emplace( key_type( context, name ), static_cast< counter_id_type >( _keys.size() ) );
      if( result.second )
        _keys.push_back( &result.first->first );
      return result.first->second;
    }

    key_type get_key( counter_id_type id )
    {
      std::lock_guard< std::mutex > guard( _mutex );
      return *_keys.at( id );
    }

  private:
    std::mutex                              _mutex;
    std::map< key_type, counter_id_type >   _ids;
    std::vector< const key_type* >          _keys; // indexed by counter id, points to keys of _ids
};

} // namespace

advanced_benchmark_dumper::counter_id_type advanced_benchmark_dumper::register_counter( const std::string& context,
  const std::string& name )
{
  return counter_registry::instance().register_counter( context, name );
}

advanced_benchmark_dumper::summary advanced_benchmark_dumper::get_summary( uint32_t block_number ) const
{
  summary result;
  result.block_number = block_number;
  for( counter_id_type id = 0; id < _counters.size(); ++id )
  {
    const counter_data& data = _counters[ id ];
    if( data.count == 0 && data.time == 0 )
      continue; // not measured by this dumper (counters are common to all dumpers)

    item i;
    std::tie( i.context, i.op_name ) = counter_registry::instance().get_key( id );
    i.time = data.time;
    i.count = data.count;
    i.time_per_count = data.count ? ( data.time + data.count - 1 ) / data.count : 0;
    result.total_time += data.time;
    result.items.emplace_back( std::move( i ) );
  }
  std::sort( result.items.begin(), result.items.end(), []( const item& i1, const item& i2 )
  {
    return std::tie( i1.op_name, i1.context ) < std::tie( i2.op_name, i2.context );
  } );
  return result;
}

void advanced_benchmark_dumper::dump( uint32_t block_number )
{
  if( _file_name.empty() )
    return;

  // never stopped - only excludes time of dumping from measurement in progress (if any)
  measurement pause( *this );

  // complete file replaces previous one, so it can be read at any time, even when it is being dumped
  const std::string temp_file_name = _file_name + ".tmp";
  try
  {
    const std::string content = fc::json::to_pretty_string( get_summary( block_number ) );

    std::ofstream file( temp_file_name, std::ios::out | std::ios::trunc );
    file << content << '\n';
    file.close();
    if( !file )
    {
      elog( "error writing benchmark data to file ${f}", ( "f", temp_file_name ) );
      return;
    }
    if( std::rename( temp_file_name.c_str(), _file_name.c_str() ) != 0 )
      elog( "error replacing benchmark data file ${f} with ${t}", ( "f", _file_name )( "t", temp_file_name ) );
  }
  catch( const fc::exception& e )
  {
    elog( "error writing benchmark data to file ${f}: ${e}", ( "f", temp_file_name )( "e", e.to_detail_string() ) );
  }
  catch( const std::exception& e )
  {
    elog( "error writing benchmark data to file ${f}: ${e}", ( "f", temp_file_name )( "e", e.what() ) );
  }
}

} } } // hive::chain::util
