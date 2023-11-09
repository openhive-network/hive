#pragma once
#include <hive/chain/hive_fwd.hpp>
#include <appbase/application.hpp>
#include <hive/chain/database.hpp>
#include <hive/chain/full_block.hpp>
#include <hive/chain/blockchain_worker_thread_pool.hpp>
#include <hive/plugins/chain/abstract_block_producer.hpp>
#include <hive/plugins/webserver/webserver_plugin.hpp>

#include <boost/signals2.hpp>

#define HIVE_CHAIN_PLUGIN_NAME "chain"

namespace hive {
  namespace chain {
    class sync_block_writer;
} }

namespace hive { namespace plugins { namespace chain {

class state_snapshot_provider;

namespace detail
{ 
  class chain_plugin_impl;   
  class chain_plugin_impl_deleter
  {
    public:
    void operator()( chain_plugin_impl* ) const;
  };
}

using std::unique_ptr;
using namespace appbase;
using namespace hive::chain;

namespace bfs = boost::filesystem;

using synchronization_type = boost::signals2::signal<void()>;

class chain_plugin : public plugin< chain_plugin >
{
public:
  APPBASE_PLUGIN_REQUIRES( (hive::plugins::webserver::webserver_plugin)(hive::plugins::json_rpc::json_rpc_plugin) )

  chain_plugin();
  virtual ~chain_plugin();

  bfs::path state_storage_dir() const;

  static const std::string& name() { static std::string name = HIVE_CHAIN_PLUGIN_NAME; return name; }

  virtual void set_program_options( options_description& cli, options_description& cfg ) override;
  virtual void plugin_initialize( const variables_map& options ) override;
  virtual void plugin_startup() override;
  virtual void plugin_shutdown() override;

  void register_snapshot_provider(state_snapshot_provider& provider);

  void report_state_options( const string& plugin_name, const fc::variant_object& opts );

  void connection_count_changed(uint32_t peer_count);
  enum class lock_type { boost, fc };
  bool accept_block( const std::shared_ptr< p2p_block_flow_control >& p2p_block_ctrl, bool currently_syncing );
  void accept_transaction( const full_transaction_ptr& trx, const lock_type lock = lock_type::boost );
  void determine_encoding_and_accept_transaction( full_transaction_ptr& result, const hive::protocol::signed_transaction& trx,
    std::function< void( bool hf26_auth_fail )> on_full_trx = []( bool ){}, const lock_type lock = lock_type::boost );
  void push_transaction( const std::shared_ptr<full_transaction_type>& full_transaction, uint32_t skip = database::skip_nothing );
  bool push_block( const block_flow_control& block_ctrl, uint32_t skip = database::skip_nothing );
  void generate_block( const std::shared_ptr< generate_block_flow_control >& generate_block_ctrl );

  /**
    * Set a class to be called for block generation.
    *
    * This function must be called during abtract_plugin::plugin_initialize().
    * Calling this during abstract_plugin::plugin_startup() will be too late
    * and will not take effect.
    */
  void register_block_generator( const std::string& plugin_name, std::shared_ptr< abstract_block_producer > block_producer );

  /**
    * Sets the time (in ms) that the write thread will hold the lock for.
    * A time of -1 is no limit and pre-empts all readers. A time of 0 will
    * only ever hold to lock for a single write before returning to readers.
    * By default, this value is 500 ms.
    *
    * This value cannot be changed once the plugin is started.
    *
    * The old value is returned so that plugins can respect overrides from
    * other plugins.
    */
  int16_t set_write_lock_hold_time( int16_t new_time );

  void check_time_in_block( const hive::chain::signed_block& block );

  template< typename MultiIndexType >
  bool has_index() const
  {
    return db().has_index< MultiIndexType >();
  }

  template< typename MultiIndexType >
  const chainbase::generic_index< MultiIndexType >& get_index() const
  {
    return db().get_index< MultiIndexType >();
  }

  template< typename ObjectType, typename IndexedByType, typename CompatibleKey >
  const ObjectType* find( CompatibleKey&& key ) const
  {
    return db().find< ObjectType, IndexedByType, CompatibleKey >( key );
  }

  template< typename ObjectType >
  const ObjectType* find( chainbase::oid< ObjectType > key = chainbase::oid_ref< ObjectType >() )
  {
    return db().find< ObjectType >( key );
  }

  template< typename ObjectType, typename IndexedByType, typename CompatibleKey >
  const ObjectType& get( CompatibleKey&& key ) const
  {
    return db().get< ObjectType, IndexedByType, CompatibleKey >( key );
  }

  template< typename ObjectType >
  const ObjectType& get( const chainbase::oid< ObjectType >& key = chainbase::oid_ref< ObjectType >() )
  {
    return db().get< ObjectType >( key );
  }

  // Exposed for backwards compatibility. In the future, plugins should manage their own internal database
  database& db();
  const database& db() const;

  const block_read_i& block_reader() const;

  hive::chain::blockchain_worker_thread_pool& get_thread_pool();

  // Emitted when the blockchain is syncing/live.
  // This is to synchronize plugins that have the chain plugin as an optional dependency.
  synchronization_type on_sync;

  bool is_p2p_enabled() const;
  void disable_p2p() const;

private:
  std::unique_ptr< detail::chain_plugin_impl, detail::chain_plugin_impl_deleter > my;
};

} } } // hive::plugins::chain
