#pragma once
#include <chainbase/hive_fwd.hpp>
#include <hive/plugins/chain/chain_plugin.hpp>

#define HIVE_ACCOUNT_HISTORY_PLUGIN_NAME "account_history"

namespace hive { namespace plugins { namespace account_history {

namespace detail { class account_history_plugin_impl; }

using namespace appbase;
using hive::protocol::account_name_type;

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
#ifndef HIVE_ACCOUNT_HISTORY_SPACE_ID
#define HIVE_ACCOUNT_HISTORY_SPACE_ID 5
#endif

/**
  *  This plugin is designed to track a range of operations by account so that one node
  *  doesn't need to hold the full operation history in memory.
  */
class account_history_plugin : public plugin< account_history_plugin >
{
  public:
    account_history_plugin();
    virtual ~account_history_plugin();

    APPBASE_PLUGIN_REQUIRES( (hive::plugins::chain::chain_plugin) )

    static const std::string& name() { static std::string name = HIVE_ACCOUNT_HISTORY_PLUGIN_NAME; return name; }

    virtual void set_program_options(
      options_description& cli,
      options_description& cfg ) override;
    virtual void plugin_initialize( const variables_map& options ) override;
    virtual void plugin_startup() override;
    virtual void plugin_shutdown() override;

    flat_map< account_name_type, account_name_type > tracked_accounts()const; /// map start_range to end_range

  private:
    std::unique_ptr< detail::account_history_plugin_impl > my;
};

} } } //hive::plugins::account_history

