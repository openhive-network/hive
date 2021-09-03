#pragma once
#include <chainbase/forward_declarations.hpp>
#include <appbase/application.hpp>

#include <hive/plugins/chain/chain_plugin.hpp>
#include <hive/plugins/block_data_export/block_data_export_plugin.hpp>

namespace hive { namespace plugins { namespace stats_export {

namespace detail { class stats_export_plugin_impl; }

using namespace appbase;

#define HIVE_STATS_EXPORT_PLUGIN_NAME "stats_export"

class stats_export_plugin : public appbase::plugin< stats_export_plugin >
{
  public:
    stats_export_plugin();
    virtual ~stats_export_plugin();

    APPBASE_PLUGIN_REQUIRES(
      (hive::plugins::block_data_export::block_data_export_plugin)
      (hive::plugins::chain::chain_plugin)
    )

    static const std::string& name() { static std::string name = HIVE_STATS_EXPORT_PLUGIN_NAME; return name; }

    virtual void set_program_options( options_description& cli, options_description& cfg ) override;
    virtual void plugin_initialize( const variables_map& options ) override;
    virtual void plugin_startup() override;
    virtual void plugin_shutdown() override;

  private:
    std::unique_ptr< detail::stats_export_plugin_impl > my;
};

} } } // hive::plugins::stats_export
