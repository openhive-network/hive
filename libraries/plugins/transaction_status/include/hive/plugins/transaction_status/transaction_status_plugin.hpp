#pragma once
#include <hive/chain/hive_fwd.hpp>
#include <hive/plugins/chain/chain_plugin.hpp>

#include <appbase/application.hpp>

namespace hive { namespace plugins { namespace transaction_status {

#define HIVE_TRANSACTION_STATUS_PLUGIN_NAME "transaction_status"

namespace detail { class transaction_status_impl; }

class transaction_status_plugin : public appbase::plugin< transaction_status_plugin >
{
  public:
    transaction_status_plugin( appbase::application& app );
    virtual ~transaction_status_plugin();

    APPBASE_PLUGIN_REQUIRES( (hive::plugins::chain::chain_plugin) )

    static const std::string& name() { static std::string name = HIVE_TRANSACTION_STATUS_PLUGIN_NAME; return name; }

    virtual void set_program_options( boost::program_options::options_description& cli, boost::program_options::options_description& cfg ) override;
    virtual void plugin_initialize( const boost::program_options::variables_map& options ) override;
    virtual void plugin_startup() override;
    virtual void plugin_shutdown() override;

    uint32_t earliest_tracked_block_num();

#ifdef IS_TEST_NET
    bool     state_is_valid();
    void     rebuild_state();
#endif

  private:
    std::unique_ptr< detail::transaction_status_impl > my;
};

} } } // hive::plugins::transaction_status
