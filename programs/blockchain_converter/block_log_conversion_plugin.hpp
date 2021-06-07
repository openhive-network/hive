#pragma once

#include <appbase/application.hpp>

#include <boost/program_options.hpp>

#include <memory>

#define HIVE_BLOCK_LOG_CONVERSION_PLUGIN_NAME "block_log_conversion"

namespace bpo = boost::program_options;

namespace hive { namespace converter { namespace plugins { namespace block_log_conversion {

namespace detail { class block_log_conversion_plugin_impl; }

class block_log_conversion_plugin final : public appbase::plugin< block_log_conversion_plugin >
{
public:
  APPBASE_PLUGIN_REQUIRES()

  block_log_conversion_plugin();
  virtual ~block_log_conversion_plugin();

  static const std::string& name() { static std::string name = HIVE_BLOCK_LOG_CONVERSION_PLUGIN_NAME; return name; }

  virtual void set_program_options( bpo::options_description& cli, bpo::options_description& cfg ) override;
  virtual void plugin_initialize( const bpo::variables_map& options ) override;
  virtual void plugin_startup() override;
  virtual void plugin_shutdown() override;

private:
  std::unique_ptr< detail::block_log_conversion_plugin_impl > my;
};

} } } } // hive::converter::plugins::block_log_conversion
