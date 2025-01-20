#pragma once
#include <hive/chain/hive_fwd.hpp>

#include <hive/plugins/chain/chain_plugin.hpp>

#define HIVE_COLONY_PLUGIN_NAME "colony"


namespace hive { namespace plugins { namespace colony {

namespace detail { class colony_plugin_impl; }

using namespace appbase;

class colony_plugin : public appbase::plugin< colony_plugin >
{
  public:
    colony_plugin();
    virtual ~colony_plugin();

    APPBASE_PLUGIN_REQUIRES( (hive::plugins::chain::chain_plugin) )

    static const std::string& name() { static std::string name = HIVE_COLONY_PLUGIN_NAME; return name; }

    virtual void set_program_options(
      options_description& cli,
      options_description& cfg ) override;
    virtual void plugin_initialize( const variables_map& options ) override;
    virtual void plugin_startup() override;
    virtual void plugin_pre_shutdown() override;
    virtual void plugin_shutdown() override;

  private:
    std::unique_ptr< detail::colony_plugin_impl > my;
};

} } } //hive::plugins::colony
