#pragma once
#include <hive/chain/hive_fwd.hpp>
#include <hive/plugins/chain/chain_plugin.hpp>

#include <hive/chain/hive_object_types.hpp>

//
// Plugins should #define their SPACE_ID's so plugins with
// conflicting SPACE_ID assignments can be compiled into the
// same binary (by simply re-assigning some of the conflicting #defined
// SPACE_ID's in a build script).
//
// Assignment of SPACE_ID's cannot be done at run-time because
// various template automagic depends on them being known at compile
// time.
//
#ifndef HIVE_MARKET_HISTORY_PLUGIN_NAME
#define HIVE_MARKET_HISTORY_PLUGIN_NAME "market_history"
#endif


namespace hive { namespace plugins { namespace market_history {

using namespace hive::chain;
using namespace appbase;

namespace detail { class market_history_plugin_impl; }

class market_history_plugin : public plugin< market_history_plugin >
{
  public:
    market_history_plugin();
    virtual ~market_history_plugin();

    APPBASE_PLUGIN_REQUIRES( (hive::plugins::chain::chain_plugin) )

    static const std::string& name() { static std::string name = HIVE_MARKET_HISTORY_PLUGIN_NAME; return name; }

    const flat_set< uint32_t >& get_tracked_buckets() const;
    uint32_t get_max_history_per_bucket() const;

    virtual void set_program_options(
      options_description& cli,
      options_description& cfg ) override;

  protected:
    virtual void plugin_initialize( const variables_map& options ) override;
    virtual void plugin_startup() override;
    virtual void plugin_shutdown() override;

  private:
    friend class detail::market_history_plugin_impl;
    std::unique_ptr< detail::market_history_plugin_impl > my;
};

} } } // hive::plugins::market_history
