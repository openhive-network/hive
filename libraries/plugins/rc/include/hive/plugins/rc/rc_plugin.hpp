#pragma once
#include <hive/chain/hive_fwd.hpp>
#include <appbase/application.hpp>

#include <hive/plugins/chain/chain_plugin.hpp>

namespace hive { namespace chain { class account_object; } }

namespace hive { namespace plugins { namespace rc {

namespace detail {

class rc_plugin_impl;

//ABW: temporarily exposed for unit tests - that functionality should become part of account_object
int64_t get_next_vesting_withdrawal( const hive::chain::account_object& account );

}

using namespace appbase;

#define HIVE_RC_PLUGIN_NAME "rc"

struct rc_plugin_skip_flags
{
  uint32_t skip_reject_not_enough_rc       : 1;
  uint32_t skip_deduct_rc                  : 1;
  uint32_t skip_negative_rc_balance        : 1;
  uint32_t skip_reject_unknown_delta_vests : 1;
};

class rc_plugin : public appbase::plugin< rc_plugin >
{
  public:
    rc_plugin();
    virtual ~rc_plugin();

    APPBASE_PLUGIN_REQUIRES( (hive::plugins::chain::chain_plugin) )

    static const std::string& name() { static std::string name = HIVE_RC_PLUGIN_NAME; return name; }

    virtual void set_program_options( options_description& cli, options_description& cfg ) override;
    virtual void plugin_initialize( const variables_map& options ) override;
    virtual void plugin_startup() override;
    virtual void plugin_shutdown() override;

    void set_rc_plugin_skip_flags( rc_plugin_skip_flags skip );
    const rc_plugin_skip_flags& get_rc_plugin_skip_flags() const;

    void validate_database();

  private:
    std::unique_ptr< detail::rc_plugin_impl > my;
};

} } } // hive::plugins::rc
