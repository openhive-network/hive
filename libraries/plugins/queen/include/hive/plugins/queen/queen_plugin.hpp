#pragma once
#include <hive/chain/hive_fwd.hpp>

#include <hive/plugins/chain/chain_plugin.hpp>
#include <hive/plugins/witness/witness_plugin.hpp>

#define HIVE_QUEEN_PLUGIN_NAME "queen"


namespace hive { namespace plugins { namespace queen {

namespace detail { class queen_plugin_impl; }

using namespace appbase;

class queen_plugin : public appbase::plugin< queen_plugin >
{
  public:
    queen_plugin();
    virtual ~queen_plugin();

    APPBASE_PLUGIN_REQUIRES( (hive::plugins::chain::chain_plugin)(hive::plugins::witness::witness_plugin) )

    static const std::string& name() { static std::string name = HIVE_QUEEN_PLUGIN_NAME; return name; }

    virtual void set_program_options(
      options_description& cli,
      options_description& cfg ) override;
    virtual void plugin_initialize( const variables_map& options ) override;
    virtual void plugin_startup() override;
    virtual void plugin_shutdown() override;

  private:
    std::unique_ptr< detail::queen_plugin_impl > my;
};

} } } //hive::plugins::queen
