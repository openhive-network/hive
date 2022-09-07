#pragma once

#include <appbase/application.hpp>

#include <boost/program_options.hpp>

#include <memory>
#include <string>

#define HIVE_NODE_BASED_CONVERSION_PLUGIN_NAME "node_based_conversion"

namespace hive { namespace converter { namespace plugins { namespace node_based_conversion {

  namespace bpo = boost::program_options;

namespace detail { class node_based_conversion_plugin_impl; }

  class node_based_conversion_plugin final : public appbase::plugin< node_based_conversion_plugin >
  {
  public:
    APPBASE_PLUGIN_REQUIRES()

    node_based_conversion_plugin();
    virtual ~node_based_conversion_plugin();

    static const std::string& name() { static std::string name = HIVE_NODE_BASED_CONVERSION_PLUGIN_NAME; return name; }

    virtual void set_program_options( bpo::options_description& cli, bpo::options_description& cfg ) override;
    virtual void plugin_initialize( const bpo::variables_map& options ) override;
    virtual void plugin_startup() override;
    virtual void plugin_shutdown() override;

  private:
    std::unique_ptr< detail::node_based_conversion_plugin_impl > my;
  };

} } } } // hive::converter::plugins::block_log_conversion
