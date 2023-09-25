#include <hive/plugins/statsd/utility.hpp>

namespace hive { namespace plugins{ namespace statsd { namespace util {

bool statsd_enabled( appbase::application& app )
{
  static bool enabled = app.find_plugin< statsd_plugin >() != nullptr;
  return enabled;
}

const statsd_plugin& get_statsd( appbase::application& app )
{
  static const statsd_plugin& statsd = app.get_plugin< statsd_plugin >();
  return statsd;
}

} } } } // hive::plugins::statsd::util
