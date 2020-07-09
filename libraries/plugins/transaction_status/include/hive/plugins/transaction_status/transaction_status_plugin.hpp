#pragma once
#include <hive/chain/hive_fwd.hpp>
#include <hive/plugins/chain/chain_plugin.hpp>

#include <appbase/application.hpp>

namespace hive { namespace plugins { namespace transaction_status {

#define HIVE_TRANSACTION_STATUS_PLUGIN_NAME "transaction_status"

namespace detail { class transaction_status_impl; }

using hive::chain::transaction_id_type;

class transaction_status_plugin : public appbase::plugin< transaction_status_plugin >
{
  public:
    transaction_status_plugin();
    virtual ~transaction_status_plugin();

    APPBASE_PLUGIN_REQUIRES( (hive::plugins::chain::chain_plugin) )

    static const std::string& name() { static std::string name = HIVE_TRANSACTION_STATUS_PLUGIN_NAME; return name; }

    virtual void set_program_options( boost::program_options::options_description& cli, boost::program_options::options_description& cfg ) override;
    virtual void plugin_initialize( const boost::program_options::variables_map& options ) override;
    virtual void plugin_startup() override;
    virtual void plugin_shutdown() override;

    uint32_t get_earliest_tracked_block_num();

    fc::optional< transaction_id_type >  get_earliest_transaction_in_range( const uint32_t first_block_num, const uint32_t last_block_num );
    fc::optional< transaction_id_type >  get_latest_transaction_in_range( const uint32_t first_block_num, const uint32_t last_block_num );

  private:
    std::unique_ptr< detail::transaction_status_impl > my;
};

namespace detail {
  struct transaction_status_helper
  {
    static bool state_is_valid( chain::database& _db, transaction_status_plugin& plugin );
    static void rebuild_state( chain::database& _db, transaction_status_plugin& plugin );
  };
}

} } } // hive::plugins::transaction_status
