#pragma once
#include <hive/chain/hive_fwd.hpp>
#include <appbase/application.hpp>

#include <hive/chain/rc/rc_utility.hpp>

#include <hive/plugins/chain/chain_plugin.hpp>

namespace hive { namespace chain { class account_object; } }

namespace hive { namespace plugins { namespace rc {

namespace detail {

class rc_plugin_impl;

}

using namespace appbase;

#define HIVE_RC_PLUGIN_NAME "rc"

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

    bool is_active() const; ///< tells if RC already started

    void set_enable_rc_stats( bool enable = true );
    bool is_rc_stats_enabled() const;

    void validate_database();

    void update_rc_for_custom_action( std::function<void()>&& callback, const protocol::account_name_type& account_name ) const;

    fc::variant_object get_report( hive::chain::resource_credits::report_type rt, bool pending = false ) const;

  private:
    std::unique_ptr< detail::rc_plugin_impl > my;
};

} } } // hive::plugins::rc
