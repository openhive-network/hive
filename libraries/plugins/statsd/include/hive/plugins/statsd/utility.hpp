#pragma once
#include <hive/plugins/statsd/statsd_plugin.hpp>

#include <fc/optional.hpp>
#include <fc/time.hpp>

namespace hive { namespace plugins { namespace statsd { namespace util {

using hive::plugins::statsd::statsd_plugin;

bool statsd_enabled( appbase::application& app );
const statsd_plugin& get_statsd( appbase::application& app );

class statsd_timer_helper
{
  public:
    statsd_timer_helper( const std::string& ns, const std::string& stat, const std::string& key, float freq, const statsd_plugin& statsd ) :
      _ns( ns ),
      _stat( stat ),
      _key( key ),
      _freq( freq ),
      _statsd( statsd )
    {
      _start = fc::time_point::now();
    }

    ~statsd_timer_helper()
    {
      record();
    }

    void record()
    {
      fc::time_point stop = fc::time_point::now();
      if( !_recorded )
      {
        _statsd.timing( _ns, _stat, _key, (stop - _start).count() / 1000 , _freq );
        _recorded = true;
      }
    }

  private:
    std::string          _ns;
    std::string          _stat;
    std::string          _key;
    float                _freq = 1.0f;
    fc::time_point       _start;
    const statsd_plugin& _statsd;
    bool                 _recorded = false;
};

inline uint32_t timing_helper( const fc::microseconds& time ) { return time.count() / 1000; }
inline uint32_t timing_helper( const fc::time_point& time ) { return time.time_since_epoch().count() / 1000; }
inline uint32_t timing_helper( const fc::time_point_sec& time ) { return time.sec_since_epoch() * 1000; }
inline uint32_t timing_helper( uint32_t time ) { return time; }

} } } } // hive::plugins::statsd::util

#define STATSD_INCREMENT( NAMESPACE, STAT, KEY, FREQ, APP )   \
if( hive::plugins::statsd::util::statsd_enabled( APP ) )     \
{                                                        \
  hive::plugins::statsd::util::get_statsd( APP ).increment( \
    NAMESPACE, STAT, KEY, FREQ                         \
  );                                                    \
}

#define STATSD_DECREMENT( NAMESPACE, STAT, KEY, FREQ, APP )   \
if( hive::plugins::statsd::util::statsd_enabled( APP ) )     \
{                                                        \
  hive::plugins::statsd::util::get_statsd( APP ).decrement( \
    NAMESPACE, STAT, KEY, FREQ                         \
  );                                                    \
}

#define STATSD_COUNT( NAMESPACE, STAT, KEY, VAL, FREQ, APP )  \
if( hive::plugins::statsd::util::statsd_enabled( APP ) )     \
{                                                        \
  hive::plugins::statsd::util::get_statsd( APP ).count(     \
    NAMESPACE, STAT, KEY, VAL, FREQ                    \
  );                                                    \
}

#define STATSD_GAUGE( NAMESPACE, STAT, KEY, VAL, FREQ, APP )  \
if( hive::plugins::statsd::util::statsd_enabled( APP ) )     \
{                                                        \
  hive::plugins::statsd::util::get_statsd( APP ).gauge(     \
    NAMESPACE, STAT, KEY, VAL, FREQ                    \
  );                                                    \
}

// You can only have one statsd timer in the current scope at a time
#define STATSD_START_TIMER( NAMESPACE, STAT, KEY, FREQ, APP )                         \
fc::optional< hive::plugins::statsd::util::statsd_timer_helper > statsd_timer;  \
if( hive::plugins::statsd::util::statsd_enabled( APP ) )                             \
{                                                                                \
  statsd_timer = hive::plugins::statsd::util::statsd_timer_helper(             \
    NAMESPACE, STAT, KEY, FREQ, hive::plugins::statsd::util::get_statsd( APP )     \
  );                                                                            \
}

#define STATSD_STOP_TIMER( NAMESPACE, STAT, KEY )        \
  statsd_timer.reset();

#define STATSD_TIMER( NAMESPACE, STAT, KEY, VAL, FREQ, APP )  \
if( hive::plugins::statsd::util::statsd_enabled( APP ) )     \
{                                                        \
  hive::plugins::statsd::util::get_statsd( APP ).timing(    \
    NAMESPACE, STAT, KEY,                              \
    hive::plugins::statsd::util::timing_helper( VAL ),\
    FREQ                                               \
  );                                                    \
}
