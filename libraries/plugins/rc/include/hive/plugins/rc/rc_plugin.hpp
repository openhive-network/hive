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
  //if set it disables check if payer has enough RC mana; node that has it set is not allowed to produce blocks or broadcast transactions
  uint32_t skip_reject_not_enough_rc       : 1; //should be false for mainnet and true for unit tests
  //if set the RC bug caused by missing proper update of last_max_rc in some operation will not stop the node
  uint32_t skip_reject_unknown_delta_vests : 1; //should be true for mainnet and false for unit tests
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
    void set_enable_rc_stats( bool enable = true );
    bool is_rc_stats_enabled() const;

    void validate_database();

    void update_rc_for_custom_action( std::function<void()>&& callback, const protocol::account_name_type& account_name ) const;

    enum class report_type
    {
      NONE, //no report
      MINIMAL, //just basic stats - no operation or payer stats
      REGULAR, //no detailed operation stats
      FULL //everything
    };
    fc::variant_object get_report( report_type rt, bool pending = false ) const;

  private:
    std::unique_ptr< detail::rc_plugin_impl > my;
};

} } } // hive::plugins::rc
