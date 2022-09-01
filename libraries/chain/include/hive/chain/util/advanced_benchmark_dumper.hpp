#pragma once

#include <fc/time.hpp>
#include <fc/variant.hpp>
#include <fc/reflect/variant.hpp>
#include <fc/exception/exception.hpp>
#include <fc/io/json.hpp>

#include <sys/time.h>

#include <utility>

namespace hive { namespace chain { namespace util {

template <typename TCntr>
struct emplace_ret_value
{
  using type = decltype(std::declval<TCntr>().emplace(std::declval<typename TCntr::value_type>()));
};


class advanced_benchmark_dumper
{                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           
  public:

    struct item
    {
      std::string context;
      std::string op_name;
      mutable uint64_t time = 0;
      mutable uint64_t count = 0;
      mutable uint64_t time_per_count = 0; //calculated only during dump
      mutable uint64_t broken_measurements = 0; //increased when nested measurement caused this outer one to reset
        //broken measurements don't have any influence on collected time or count

      item( std::string _context, std::string _op_name, uint64_t _time, uint64_t _count = 1, bool broken = false )
        : context( std::move(_context) ), op_name( std::move(_op_name) )
      {
        inc( time, _count, broken );
      }

      bool operator<( const item& obj ) const
      {
        if( op_name < obj.op_name )
          return true;
        else
          return ( op_name == obj.op_name ) && ( context < obj.context );
      }
      void inc( uint64_t _time, uint64_t _count = 1, bool broken = false ) const
      {
        if( broken )
        {
          broken_measurements += _count;
        }
        else
        {
          time += _time;
          count += _count;
        }
      }
      void calculate_time_per_count() const { time_per_count = count ? ( time + count - 1 ) / count : 0; } //round up
    };

    struct ritem
    {
      std::string context;
      std::string op_name;
      uint64_t time;
      uint64_t count;
      uint64_t time_per_count;
      uint64_t broken_measurements;

      ritem( const item& i ) : context( i.context ), op_name( i.op_name ), time( i.time ), count( i.count ),
        time_per_count( i.time_per_count ), broken_measurements( i.broken_measurements ) {}

      bool operator<( const ritem& obj ) const { return time > obj.time; }
    };

    template< typename COLLECTION >
    struct total_info
    {
      uint64_t total_time = 0;

      COLLECTION items;
      
      total_info(){}
      total_info( uint64_t _total_time )
        : total_time( _total_time ) {}

      void inc( uint64_t _time ) { total_time += _time; }

      template <typename... TArgs>
      typename emplace_ret_value<COLLECTION>::type emplace(TArgs&&... args)
        { return items.emplace(std::forward<TArgs>(args)...); }
    };

  private:

    static uint32_t cnt;
    static std::string virtual_operation_name;
    static std::string apply_context_name;

    bool enabled = false;

    uint32_t flush_cnt = 0;
    uint32_t flush_max = 500000;

    uint64_t time_begin = 0;

    std::string file_name;

    total_info< std::set< item > > info;

    template< typename COLLECTION >
    void dump_impl( const total_info< COLLECTION >& src, const std::string& src_file_name );

  public:

    advanced_benchmark_dumper();
    ~advanced_benchmark_dumper();

    static std::string& get_virtual_operation_name(){ return virtual_operation_name; }

    template< bool IS_PRE_OPERATION >
    static std::string generate_context_desc( const std::string& desc )
    {
      std::stringstream s;
      s << ( IS_PRE_OPERATION ? "pre->" : "post->" ) << desc;

      return s.str();
    }

    void set_enabled( bool val ) { enabled = val; }
    bool is_enabled() { return enabled; }

    void begin();
    void end( const std::string& str, uint64_t _count = 1 ) { end( apply_context_name, str, _count ); }
    void end( const std::string& context, const std::string& str, uint64_t _count = 1 );

    void dump();
};

} } } // hive::chain::util

FC_REFLECT( hive::chain::util::advanced_benchmark_dumper::item, (context)(op_name)(time)(count)(time_per_count)(broken_measurements) )
FC_REFLECT( hive::chain::util::advanced_benchmark_dumper::ritem, (context)(op_name)(time)(count)(time_per_count)(broken_measurements) )

FC_REFLECT( hive::chain::util::advanced_benchmark_dumper::total_info< std::set< hive::chain::util::advanced_benchmark_dumper::item > >, (total_time)(items) )
FC_REFLECT( hive::chain::util::advanced_benchmark_dumper::total_info< std::multiset< hive::chain::util::advanced_benchmark_dumper::ritem > >, (total_time)(items) )

