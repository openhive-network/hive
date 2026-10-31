#pragma once

#include <fc/reflect/reflect.hpp>

#include <atomic>
#include <chrono>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace hive { namespace chain { namespace util {

/**
  * Collects execution times of evaluators, automatic processing and plugin reactions to signals and notifications
  * (hived run with --advanced-benchmark). Measurements of the same item in the same context are summed up in one
  * counter for the whole run - the more measurements, the more accurate the average time of the item. Data collected
  * so far is written into file every time \see dump is called.
  *
  * Single measurement is made by \see measurement object: it starts when the object is created and ends with
  * \see measurement::stop that adds the time into selected counter. Measurements can be nested, e.g. reaction of
  * a plugin to notification on virtual operation is measured inside measurement of evaluator that emitted the virtual
  * operation. The time of nested measurement is excluded from the enclosing one (the enclosing measurement is paused
  * when nested one starts and resumed when it ends), so each part of code is counted only once.
  *
  * Counters are identified by context and name of measured item. Costly part - building name and finding counter for
  * it - is done once per counter (\see register_counter, \see counter, \see variant_counters), measurements only
  * refer to counters with their numeric ids.
  *
  * Note: all measurements have to be made on one thread (the one that applies blocks and transactions).
  */
class advanced_benchmark_dumper
{
  public:
    typedef uint32_t counter_id_type;
    static constexpr counter_id_type INVALID_COUNTER = std::numeric_limits< counter_id_type >::max();

    /// Data collected in single counter
    struct item
    {
      std::string context;
      std::string op_name;
      uint64_t    time = 0; // [ns]
      uint64_t    count = 0;
      uint64_t    time_per_count = 0; // [ns] rounded up
    };

    /// Data written into file
    struct summary
    {
      uint32_t            block_number = 0; // head block at the time of the dump
      uint64_t            total_time = 0; // [ns] sum of time of all items
      std::vector< item > items; // items measured at least once, sorted by name, then by context
    };

    /**
      * Returns id of counter of given item measured in given context. Registers new counter on first call.
      * Counter ids are common to all dumpers (all of them have the same id for the same item), but each dumper
      * collects its own data.
      */
    static counter_id_type register_counter( const std::string& context, const std::string& name );

    /// Counter of item with fixed context and name (strings that live as long as the counter, e.g. literals);
    /// registered when first used
    class counter
    {
      public:
        constexpr counter( const char* context, const char* name ) : _context( context ), _name( name ) {}

        counter_id_type get_id() const
        {
          counter_id_type id = _id.load( std::memory_order_relaxed );
          if( id == INVALID_COUNTER )
          {
            id = register_counter( _context, _name );
            _id.store( id, std::memory_order_relaxed );
          }
          return id;
        }

      private:
        const char*                              _context;
        const char*                              _name;
        mutable std::atomic< counter_id_type >   _id{ INVALID_COUNTER };
    };

    /// Counters of the same context, one for every type of static_variant (e.g. operation); registered when first used
    template< typename StaticVariant >
    class variant_counters
    {
      public:
        explicit variant_counters( std::string context ) : _context( std::move( context ) ) {}

        counter_id_type get_id( const StaticVariant& item )
        {
          const size_t which = static_cast< size_t >( item.which() );
          if( which >= _ids.size() )
            _ids.resize( which + 1, INVALID_COUNTER );
          counter_id_type& id = _ids[ which ];
          if( id == INVALID_COUNTER )
            id = register_counter( _context, item.get_stored_type_name() );
          return id;
        }

      private:
        std::string                     _context;
        std::vector< counter_id_type >  _ids; // indexed by type tag
    };

    /**
      * Measurement of single item. Starts when created (only when dumper is enabled), ends with stop(). When it is
      * nested inside other measurement, the other one does not include time between start and the end of this one.
      * When measurement is not stopped (e.g. due to exception, or because there turned out to be nothing to count),
      * its time is not counted anywhere.
      */
    class measurement
    {
      public:
        explicit measurement( advanced_benchmark_dumper& dumper )
        {
          if( dumper.is_enabled() )
          {
            const int64_t start = now();
            _dumper = &dumper;
            // pause enclosing measurement (if any) - remember how much time it has collected so far
            _outer_time = ( dumper._begin != 0 ) ? start - dumper._begin : NO_OUTER_MEASUREMENT;
            dumper._begin = start;
          }
        }
        ~measurement()
        {
          if( _dumper != nullptr )
            resume_outer();
        }
        measurement( const measurement& ) = delete;
        measurement& operator=( const measurement& ) = delete;

        /// Ends measurement, adds its time to given counter as time of execution of given number of items
        void stop( counter_id_type id, uint64_t count = 1 )
        {
          if( _dumper != nullptr )
          {
            const int64_t end = now();
            finish( end, id, count );
          }
        }
        void stop( const counter& c, uint64_t count = 1 )
        {
          if( _dumper != nullptr )
          {
            const int64_t end = now(); // before registration of counter on its first use
            finish( end, c.get_id(), count );
          }
        }
        template< typename StaticVariant >
        void stop( variant_counters< StaticVariant >& counters, const StaticVariant& item )
        {
          if( _dumper != nullptr )
          {
            const int64_t end = now(); // before registration of counter on its first use
            finish( end, counters.get_id( item ), 1 );
          }
        }

      private:
        static constexpr int64_t NO_OUTER_MEASUREMENT = -1;

        void finish( int64_t end, counter_id_type id, uint64_t count )
        {
          _dumper->add( id, end - _dumper->_begin, count );
          resume_outer();
        }

        void resume_outer()
        {
          // enclosing measurement continues from the moment it was paused (time spent since then is not counted)
          _dumper->_begin = ( _outer_time == NO_OUTER_MEASUREMENT ) ? 0 : now() - _outer_time;
          _dumper = nullptr;
        }

        advanced_benchmark_dumper* _dumper = nullptr; // set only while measurement is in progress
        int64_t                    _outer_time = NO_OUTER_MEASUREMENT; // time collected by enclosing measurement
    };

    advanced_benchmark_dumper() = default;
    ~advanced_benchmark_dumper() = default;
    advanced_benchmark_dumper( const advanced_benchmark_dumper& ) = delete;
    advanced_benchmark_dumper& operator=( const advanced_benchmark_dumper& ) = delete;

    template< bool IS_PRE_OPERATION >
    static std::string generate_context_desc( const std::string& desc )
    {
      return ( IS_PRE_OPERATION ? "pre->" : "post->" ) + desc;
    }

    void set_enabled( bool val ) { _enabled = val; }
    bool is_enabled() const { return _enabled; }

    /// File overwritten by \see dump; when not set, data can only be accessed with \see get_summary
    void set_file_name( const std::string& file_name ) { _file_name = file_name; }
    const std::string& get_file_name() const { return _file_name; }

    /// Collects data of all items measured so far
    summary get_summary( uint32_t block_number ) const;
    /// Overwrites file with data of all items measured so far (time of dumping is excluded from measurement in progress)
    void dump( uint32_t block_number );

  private:
    struct counter_data
    {
      uint64_t time = 0; // [ns]
      uint64_t count = 0;
    };

    static int64_t now()
    {
      return std::chrono::duration_cast< std::chrono::nanoseconds >(
        std::chrono::steady_clock::now().time_since_epoch() ).count();
    }

    void add( counter_id_type id, int64_t time, uint64_t count )
    {
      if( id >= _counters.size() )
        _counters.resize( id + 1 );
      counter_data& data = _counters[ id ];
      data.time += time;
      data.count += count;
    }

    bool                          _enabled = false;
    std::string                   _file_name;
    std::vector< counter_data >   _counters; // indexed by counter id
    int64_t                       _begin = 0; // start of measurement in progress (shifted by time of nested ones) or 0
};

} } } // hive::chain::util

FC_REFLECT( hive::chain::util::advanced_benchmark_dumper::item, (context)(op_name)(time)(count)(time_per_count) )
FC_REFLECT( hive::chain::util::advanced_benchmark_dumper::summary, (block_number)(total_time)(items) )
