#pragma once
#include <chainbase/forward_declarations.hpp>
#include <hive/plugins/debug_node/debug_node_plugin.hpp>
#include <hive/plugins/json_rpc/json_rpc_plugin.hpp>

#include <appbase/application.hpp>

#define HIVE_DEBUG_NODE_API_PLUGIN_NAME "debug_node_api"


namespace hive { namespace plugins { namespace debug_node {

using namespace appbase;

class debug_node_api_plugin : public appbase::plugin< debug_node_api_plugin >
{
public:
  APPBASE_PLUGIN_REQUIRES(
    (hive::plugins::debug_node::debug_node_plugin)
    (hive::plugins::json_rpc::json_rpc_plugin)
  )

  debug_node_api_plugin();
  virtual ~debug_node_api_plugin();

  static const std::string& name() { static std::string name = HIVE_DEBUG_NODE_API_PLUGIN_NAME; return name; }

  virtual void set_program_options( options_description& cli, options_description& cfg ) override;

  virtual void plugin_initialize( const variables_map& options ) override;
  virtual void plugin_startup() override;
  virtual void plugin_shutdown() override;

  std::shared_ptr< class debug_node_api > api;
};

} } } // hive::plugins::debug_node
