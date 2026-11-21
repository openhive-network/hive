#include <hive/plugins/p2p/p2p_plugin.hpp>
#include <hive/plugins/p2p/p2p_default_seeds.hpp>
#include <hive/plugins/statsd/utility.hpp>

#include <graphene/net/node.hpp>
#include <graphene/net/exceptions.hpp>

#include <hive/chain/database_exceptions.hpp>
#include <hive/chain/blockchain_worker_thread_pool.hpp>

#include <appbase/shutdown_mgr.hpp>

#include <fc/network/ip.hpp>
#include <fc/network/resolve.hpp>
#include <fc/thread/thread.hpp>
#include <fc/io/json.hpp>

#include <boost/range/algorithm/reverse.hpp>
#include <boost/range/adaptor/reversed.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/preprocessor/stringize.hpp>

#include <boost/any.hpp>

#include <chrono>

using std::string;
using std::vector;

namespace hive { namespace plugins { namespace p2p {

using graphene::net::item_hash_t;
using graphene::net::item_id;
using graphene::net::message;
using graphene::net::block_message;
using graphene::net::trx_message;

using hive::protocol::block_header;
using hive::protocol::block_id_type;

namespace detail {

/// Reports to the p2p node, from the writer thread, how validating a transaction it received ended
class p2p_transaction_flow_control final : public chain::transaction_flow_control
{
public:
  p2p_transaction_flow_control( const std::shared_ptr<hive::chain::full_transaction_type>& _tx,
                                graphene::net::transaction_validated_callback _on_validated )
    : transaction_flow_control( _tx ), on_validated( std::move( _on_validated ) ) {}

  virtual void on_worker_done( appbase::application& app ) const override
  {
    transaction_flow_control::on_worker_done( app );
    on_validated( get_exception() );
  }

private:
  const graphene::net::transaction_validated_callback on_validated;
};

/// how often unresolved seed node hostnames are retried while the node runs
constexpr uint32_t HIVE_P2P_SEED_RESOLVE_RETRY_SECONDS = 60;

class p2p_plugin_impl : public graphene::net::node_delegate
{
public:

  p2p_plugin_impl( plugins::chain::chain_plugin& c, appbase::application& app )
    : shutdown_helper( "P2P plugin", c.get_app(), c ), chain( c ), theApp( app )
  {
  }
  virtual ~p2p_plugin_impl()
  {
    ilog("P2P plugin is closing...");
    try
    {
      shutdown_helper.shutdown();
    }
    catch (...)
    {
      // this seems like it should never happen, the plugin should always be shut down
      // via pre_shutdown_plugin().  But if we do get here, we want to make
      // sure that exceptions thrown by shutdown() are caught so the standard library
      // doesn't terminate us
      elog("Swallowing exceptions from shutdown_helper");
    }
    ilog("P2P plugin was closed...");
    theApp.notify_status("P2P stopped");
  }

  bool is_included_block(const block_id_type& block_id);
  virtual hive::protocol::chain_id_type get_new_chain_id() const override;
  virtual hive::protocol::chain_id_type get_chain_id() const override;

  // node_delegate interface
  virtual bool has_item( const graphene::net::item_id& ) override;
  virtual graphene::net::block_handling_result handle_block(const std::shared_ptr<hive::chain::full_block_type>& full_block, bool) override;
  virtual void submit_transaction(const std::shared_ptr<hive::chain::full_transaction_type>& full_transaction,
                                  graphene::net::transaction_validated_callback on_validated) override;
  virtual void handle_message( const graphene::net::message& ) override;
  virtual std::vector< graphene::net::item_hash_t > get_block_ids( const std::vector< graphene::net::item_hash_t >&, uint32_t&, uint32_t ) override;
  virtual std::shared_ptr<chain::full_block_type> get_full_block(const block_id_type&) override;
  virtual std::vector< graphene::net::item_hash_t > get_blockchain_synopsis( const graphene::net::item_hash_t&, uint32_t ) override;
  virtual void sync_status( uint32_t, uint32_t ) override;
  virtual void connection_count_changed( uint32_t ) override;
  virtual uint32_t get_block_number( const graphene::net::item_hash_t& ) override;
  virtual fc::time_point_sec get_block_time(const graphene::net::item_hash_t& ) override;
  virtual fc::time_point_sec get_blockchain_now() override;
  virtual graphene::net::item_hash_t get_head_block_id() const override;
  virtual uint32_t get_last_irreversible_block_num() const override;
  virtual uint32_t get_tail_block_num() const override;
  virtual bool is_at_live_sync() override;
  virtual graphene::net::pending_transaction_pool_size get_pending_transaction_pool_size() const override;
  virtual uint32_t estimate_last_known_fork_from_git_revision_timestamp( uint32_t ) const override;
  virtual void error_encountered( const std::string& message, const fc::oexception& error ) override;
  virtual std::deque<block_id_type>::const_iterator find_first_item_not_in_blockchain(const std::deque<block_id_type>& item_hashes_received) override;

  void request_precomputing_transaction_signatures_if_useful();

  fc::optional<fc::ip::endpoint> endpoint;
  vector<fc::ip::endpoint> seeds;
  /// seed node strings that did not resolve during initialization (e.g. no DNS yet); retried
  /// periodically on the p2p thread once the network is up, see retry_unresolved_seeds_loop()
  vector<string> unresolved_seeds;
  fc::future<void> retry_unresolved_seeds_task;
  void retry_unresolved_seeds_loop();
  string user_agent;
  fc::mutable_variant_object config;
  uint32_t max_connections = 0;
  fc::optional<uint32_t> transaction_flush_delay_us;
  fc::optional<uint32_t> fetch_items_minimum_interval_us;
  fc::optional<uint32_t> max_transactions_in_flight;
  fc::optional<uint32_t> max_connections_per_host;
  vector<string> host_limit_exempt;
  bool force_validate = false;
  bool block_producer = false;

  shutdown_mgr shutdown_helper;

  std::unique_ptr<graphene::net::node> node;

  plugins::chain::chain_plugin& chain;

  fc::thread p2p_thread;

  appbase::application& theApp;
};

////////////////////////////// Begin node_delegate Implementation //////////////////////////////
bool p2p_plugin_impl::has_item( const graphene::net::item_id& id )
{
  try
  {
    if( id.item_type == graphene::net::block_message_type )
      return chain.block_reader().is_known_block(id.item_hash);
    else
      return chain.db().with_read_lock( [&]() {
        return chain.db().is_known_transaction(id.item_hash);
      });
  }
  FC_CAPTURE_LOG_AND_RETHROW( (id) )
}

graphene::net::block_handling_result p2p_plugin_impl::handle_block(const std::shared_ptr<hive::chain::full_block_type>& full_block, bool sync_mode)
{ try {
  if( shutdown_helper.is_running() )
  {
    action_catcher ac( shutdown_helper, hive_p2p_handler_type::HIVE_P2P_BLOCK_HANDLER );

    uint32_t head_block_num = chain.block_reader().head_block_num();
    if (sync_mode)
      fc_ilog(fc::logger::get("sync"),
          "chain pushing sync block #${block_num} ${block_hash}, head is ${head_block_num}",
          ("block_num", full_block->get_block_num())
          ("block_hash", full_block->get_block_id())
          (head_block_num));
    else
      fc_ilog(fc::logger::get("sync"),
          "chain pushing block #${block_num} ${block_hash}, head is ${head_block_num}",
          ("block_num", full_block->get_block_num())
          ("block_hash", full_block->get_block_id())
          (head_block_num));

    try
    {
      // TODO: in the case where this block is valid but on a fork that's too old for us to switch to,
      // you can help the network code out by throwing a block_older_than_undo_history exception.
      // when the net code sees that, it will stop trying to push blocks from that chain, but
      // leave that peer connected so that they can get sync blocks from us
      graphene::net::block_handling_result result = graphene::net::block_handling_result::applied;
      {
        uint32_t skip_flags = (block_producer | force_validate) ? chain::database::skip_nothing : chain::database::skip_transaction_signatures;
        std::shared_ptr<chain::p2p_block_flow_control> p2p_block_ctrl;
        if (sync_mode)
          p2p_block_ctrl = std::make_shared<chain::sync_block_flow_control>(full_block, skip_flags);
        else
          p2p_block_ctrl = std::make_shared<chain::p2p_block_flow_control>(full_block, skip_flags);
        const bool switched_forks = chain.accept_block(p2p_block_ctrl, sync_mode);
        if (p2p_block_ctrl->ignored())
          result = graphene::net::block_handling_result::ignored;
        else if (switched_forks)
          result = graphene::net::block_handling_result::switched_forks;
      }

      if (!sync_mode)
      {
        fc::microseconds offset = fc::time_point::now() - full_block->get_block_header().timestamp;
        STATSD_TIMER("p2p", "offset", "block_arrival", offset, 1.0f, theApp)
        ilog("Got ${t} transactions on block ${b} by ${w} -- Block Time Offset: ${l} ms",
             ("t", full_block->get_block().transactions.size())
             ("b", full_block->get_block_num())
             ("w", full_block->get_block_header().witness)
             ("l", offset.count() / 1000));
      }

      return result;
    }
    catch (const chain::unlinkable_block_exception& e)
    {
      // translate to a graphene::net exception
      // this is already dlogged() to the default logger inside fork_database::push_block(),
      // additionally, we log it to the sync logger here
      fc_dlog(fc::logger::get("sync"), "Error when pushing block:\n${e}", ("e", e.to_detail_string()));
      FC_THROW_EXCEPTION(graphene::net::unlinkable_block_exception, "Error when pushing block:\n${e}", ("e", e.to_detail_string()));
    }
    catch (const chain::block_body_mismatch_exception& e)
    {
      // translate to a graphene::net exception, so the node blames only the peer that sent this copy
      fc_wlog(fc::logger::get("sync"), "Error when pushing block:\n${e}", ("e", e.to_detail_string()));
      FC_THROW_EXCEPTION(graphene::net::block_body_mismatch_exception, "Error when pushing block:\n${e}", ("e", e.to_detail_string()));
    }
    catch (const fc::exception& e)
    {
      //fc_elog(fc::logger::get("sync"), "Error when pushing block:\n${e}", ("e", e.to_detail_string()));
      //elog("Error when pushing block:\n${e}", ("e", e.to_detail_string()));
      if (e.code() == 4080000)
      {
        elog("Rethrowing as graphene::net exception");
        FC_THROW_EXCEPTION(graphene::net::unlinkable_block_exception, "Error when pushing block:\n${e}", ("e", e.to_detail_string()));
      }
      else if (e.code() == chain::block_body_mismatch_exception::code_value)
      {
        elog("Rethrowing as graphene::net exception");
        FC_THROW_EXCEPTION(graphene::net::block_body_mismatch_exception, "Error when pushing block:\n${e}", ("e", e.to_detail_string()));
      }
      else
      {
        throw;
      }
    }
  }
  else
  {
    dlog("Block ignored due to start p2p_plugin shutdown");
    FC_THROW_EXCEPTION(graphene::net::p2p_node_shutting_down_exception, "Preventing further processing of ignored block...");
  }
  return graphene::net::block_handling_result::ignored;
} FC_CAPTURE_AND_RETHROW((sync_mode)) }

void p2p_plugin_impl::submit_transaction(const std::shared_ptr<hive::chain::full_transaction_type>& full_transaction,
                                         graphene::net::transaction_validated_callback on_validated)
{
  if (shutdown_helper.is_running())
  {
    try
    {
      action_catcher ac( shutdown_helper, hive_p2p_handler_type::HIVE_P2P_TRANSACTION_HANDLER );

      chain.queue_transaction(std::make_shared<p2p_transaction_flow_control>(full_transaction, std::move(on_validated)),
                              chain::chain_plugin::lock_type::fc);

    } FC_CAPTURE_AND_RETHROW((full_transaction->get_transaction()))
  }
  else
  {
    ilog("Transaction ignored due to start p2p_plugin shutdown");
    FC_THROW("Preventing further processing of ignored transaction...");
  }
}

void p2p_plugin_impl::handle_message( const graphene::net::message& message_to_process )
{
  // not a transaction, not a block
  FC_THROW( "Invalid Message Type" );
}

std::vector<graphene::net::item_hash_t> p2p_plugin_impl::get_block_ids(const std::vector< graphene::net::item_hash_t >& blockchain_synopsis, uint32_t& remaining_item_count, uint32_t limit)
{ try {
  try
  {
    return chain.block_reader().get_block_ids(blockchain_synopsis, remaining_item_count, limit);
  }
  catch (const hive::chain::internal_peer_is_on_an_unreachable_fork&)
  {
    FC_THROW_EXCEPTION(graphene::net::peer_is_on_an_unreachable_fork, "Unable to provide a list of blocks starting at any of the blocks in peer's synopsis");
  }
  catch (const hive::chain::internal_peer_needs_pruned_blocks&)
  {
    FC_THROW_EXCEPTION(graphene::net::peer_needs_pruned_blocks, "Unable to provide a list of blocks the peer needs, they have been pruned");
  }
} FC_CAPTURE_AND_RETHROW((blockchain_synopsis)(remaining_item_count)(limit)) }

std::shared_ptr<chain::full_block_type> p2p_plugin_impl::get_full_block(const block_id_type& id)
{ try {
  fc_dlog(fc::logger::get("chainlock"), "get_full_block will get forkdb read lock");
  return chain.block_reader().fetch_block_by_id(id);
} FC_CAPTURE_AND_RETHROW((id)) }

hive::protocol::chain_id_type p2p_plugin_impl::get_new_chain_id() const
{
  return chain.db().get_new_chain_id();
}

hive::protocol::chain_id_type p2p_plugin_impl::get_chain_id() const
{
  return chain.db().get_chain_id();
}

std::vector< graphene::net::item_hash_t > p2p_plugin_impl::get_blockchain_synopsis( const graphene::net::item_hash_t& reference_point, uint32_t number_of_blocks_after_reference_point )
{ try {
  try
  {
    return chain.block_reader().get_blockchain_synopsis(reference_point, number_of_blocks_after_reference_point);
  }
  catch (const hive::chain::internal_peer_is_on_an_unreachable_fork&)
  {
    // translate to the exception type the network layer's synopsis-construction path handles:
    // it disconnects the peer with a clear "you are on a fork I'm unable to switch to" reason
    // instead of letting a generic exception escape its handling
    FC_THROW_EXCEPTION(graphene::net::block_older_than_undo_history, "Unable to construct a blockchain synopsis for this peer; it is on a fork we cannot reconcile with");
  }
  catch (const hive::chain::internal_synopsis_reference_point_pruned&)
  {
    FC_THROW_EXCEPTION(graphene::net::synopsis_reference_point_pruned, "Unable to construct a blockchain synopsis for this peer; the block it would start from has been pruned");
  }
} FC_CAPTURE_AND_RETHROW((reference_point)(number_of_blocks_after_reference_point)) }

void p2p_plugin_impl::sync_status( uint32_t item_type, uint32_t item_count )
{
  // any status reports to GUI go here
}

void p2p_plugin_impl::connection_count_changed( uint32_t peer_count )
{
  // any status reports to GUI go here
  chain.connection_count_changed(peer_count);
}

uint32_t p2p_plugin_impl::get_block_number( const graphene::net::item_hash_t& block_id )
{ try {
  return block_header::num_from_id(block_id);
} FC_CAPTURE_AND_RETHROW( (block_id) ) }

fc::time_point_sec p2p_plugin_impl::get_block_time( const graphene::net::item_hash_t& block_id )
{
  try
  {
    if( block_id == graphene::net::item_hash_t() )
      return HIVE_GENESIS_TIME;
    // also known for recent blocks a fully pruned node no longer has
    const fc::optional<fc::time_point_sec> block_time =
      chain.block_reader().fetch_block_time_by_id(block_id);
    return block_time ? *block_time : fc::time_point_sec::min();
  } FC_CAPTURE_AND_RETHROW((block_id))
}

graphene::net::item_hash_t p2p_plugin_impl::get_head_block_id() const
{ try {
  return chain.block_reader().head_block_id();
} FC_CAPTURE_AND_RETHROW() }

uint32_t p2p_plugin_impl::get_last_irreversible_block_num() const
{ try {
  return chain.db().get_last_irreversible_block_num();
} FC_CAPTURE_AND_RETHROW() }

uint32_t p2p_plugin_impl::get_tail_block_num() const
{ try {
  return chain.block_reader().get_tail_block_num();
} FC_CAPTURE_AND_RETHROW() }

bool p2p_plugin_impl::is_at_live_sync()
{ try {
  // same criterion chain plugin uses to switch to live mode (it honors --force-live-sync too),
  // but checked every time instead of once
  std::shared_ptr<hive::chain::full_block_type> head_block = chain.block_reader().head_block();
  fc::time_point_sec head_block_time = head_block ? head_block->get_block_header().timestamp : fc::time_point_sec::min();
  return chain.get_time_gap_to_live_sync( head_block_time ).count() < 0;
} FC_CAPTURE_AND_RETHROW() }

graphene::net::pending_transaction_pool_size p2p_plugin_impl::get_pending_transaction_pool_size() const
{
  return { chain.get_published_pending_transactions_size(), chain.get_max_mempool_size() };
}

// Runs on the p2p thread as a long-lived task while any seed string from the configuration is
// still unresolved.  Every HIVE_P2P_SEED_RESOLVE_RETRY_SECONDS it retries them; each one that
// resolves is handed to the node like a seed given at startup and dropped from the list.  A
// permanently dead seed costs one DNS lookup per interval (fc's resolver yields rather than
// blocking the thread).  The sleep is a cancellable fc::usleep, like the node's own loops, so
// plugin_pre_shutdown can cancel the task immediately rather than waiting out the interval.
void p2p_plugin_impl::retry_unresolved_seeds_loop()
{
  while( !unresolved_seeds.empty() && !theApp.is_interrupt_request() )
  {
    fc::usleep( fc::seconds(HIVE_P2P_SEED_RESOLVE_RETRY_SECONDS) );
    if( !node || theApp.is_interrupt_request() )
      return;

    for( auto iter = unresolved_seeds.begin(); iter != unresolved_seeds.end(); )
    {
      std::vector<fc::ip::endpoint> endpoints;
      try
      {
        endpoints = fc::resolve_string_to_ip_endpoints( *iter );
      }
      catch( const fc::canceled_exception& )
      {
        throw;
      }
      catch( const fc::exception& e )
      {
        dlog("seed node ${endpoint} still does not resolve: ${e}", ("endpoint", *iter)("e", e.to_detail_string()));
        ++iter;
        continue;
      }

      ilog("seed node ${endpoint} now resolves to ${count} address(es)", ("endpoint", *iter)("count", endpoints.size()));
      for( const auto& seed : endpoints )
      {
        try
        {
          ilog("P2P adding seed node ${s}", ("s", seed));
          seeds.push_back( seed );
          node->add_node( seed );
        }
        catch( graphene::net::already_connected_to_requested_peer& )
        {
          wlog( "Already connected to seed node ${s}", ("s", seed) );
        }
      }
      iter = unresolved_seeds.erase( iter );
    }
  }
}

uint32_t p2p_plugin_impl::estimate_last_known_fork_from_git_revision_timestamp(uint32_t) const
{
  return 0; // there are no forks in graphene
}

void p2p_plugin_impl::error_encountered( const string& message, const fc::oexception& error )
{
  // notify GUI or something cool
}

std::deque<block_id_type>::const_iterator p2p_plugin_impl::find_first_item_not_in_blockchain(const std::deque<block_id_type>& item_hashes_received)
{
  return chain.block_reader().find_first_item_not_in_blockchain(item_hashes_received);
}

void p2p_plugin_impl::request_precomputing_transaction_signatures_if_useful()
{
  if (force_validate)
    chain.get_thread_pool().set_p2p_force_validate();
  if (block_producer)
    chain.get_thread_pool().set_is_block_producer();
}

fc::time_point_sec p2p_plugin_impl::get_blockchain_now()
{ try {
  return fc::time_point::now();
} FC_CAPTURE_AND_RETHROW() }

bool p2p_plugin_impl::is_included_block(const block_id_type& block_id)
{ try {
  uint32_t block_num = block_header::num_from_id(block_id);
  try
  {
    block_id_type block_id_in_preferred_chain = 
      chain.block_reader().find_block_id_for_num(block_num);
    return block_id == block_id_in_preferred_chain;
  }
  catch (fc::key_not_found_exception&)
  {
    return false;
  }
} FC_CAPTURE_AND_RETHROW() }

////////////////////////////// End node_delegate Implementation //////////////////////////////

} // detail

p2p_plugin::p2p_plugin()
{
  set_pre_shutdown_order( p2p_order );
}

p2p_plugin::~p2p_plugin() {}

void p2p_plugin::set_program_options( bpo::options_description& cli, bpo::options_description& cfg)
{
  std::stringstream seed_ss;
  for( auto& s : default_seeds )
  {
    seed_ss << s << ' ';
  }

  cfg.add_options()
    ("p2p-endpoint", bpo::value<string>()->implicit_value("127.0.0.1:9876"), "The local IP address and port to listen for incoming connections.")
    ("p2p-max-connections", bpo::value<uint32_t>(), "Maxmimum number of incoming connections on P2P endpoint.")
    ("p2p-seed-node", bpo::value<vector<string>>()->composing()->default_value( default_seeds, seed_ss.str() ), "The IP address and port of a remote peer to sync with. A host name is resolved to all of its addresses and each becomes "
     "its own peer entry, so a host with both IPv4 and IPv6 addresses is dialed twice; the second handshake is rejected as "
     "already connected once per startup, which is expected and does not delay later connection attempts.")
    ("p2p-transaction-flush-delay-us", bpo::value<uint32_t>(), "Maximum time, in microseconds, that transaction traffic queued for a peer waits for more to go out in the same socket write. "
     "A queued block, sync or control message, or a full batch, ends the wait at once. Default 1000 (1 ms); 0 disables the delay and writes without waiting. "
     "Overrides transaction_flush_delay_microseconds in p2p-parameters.")
    ("p2p-fetch-items-minimum-interval-us", bpo::value<uint32_t>(), "Minimum time, in microseconds, between two passes over the items to fetch from peers. "
     "Transaction advertisements arriving within it are handled together in the next pass; a block to fetch starts a pass at once. "
     "Default 5000 (5 ms); 0 starts a pass on every advertisement. Overrides fetch_items_minimum_interval_microseconds in p2p-parameters.")
    ("p2p-max-transactions-in-flight", bpo::value<uint32_t>(), "Maximum number of transactions received from peers that may wait for validation at once. "
     "At the limit, the next transaction a peer sends waits until one is validated, and so does reading anything further from that peer. "
     "Default 10000; 0 for no limit. Overrides maximum_transactions_in_flight in p2p-parameters.")
    ("p2p-max-connections-per-host", bpo::value<uint32_t>(), "Maximum number of connections held to one host, an IPv4 address or an IPv6 /64, inbound and outbound together. "
     "A hello beyond it is rejected, and the peer counts that as a failed attempt and backs off. Loopback addresses and p2p-host-limit-exempt "
     "ones are not limited, and neither are the seed nodes and add_node targets this node dials (they still count against their host's limit). "
     "Behind a proxy that rewrites the source address of inbound peers (docker-proxy, rootless Docker) every inbound peer is on the proxy's "
     "host: exempt the proxy's address or set 0. "
     "Left unset (here and as maximum_connections_per_host in p2p-parameters), the default of " BOOST_PP_STRINGIZE(GRAPHENE_NET_DEFAULT_MAX_CONNECTIONS_PER_HOST)
     " gives way while peers on other hosts can't fill the desired connections, and the connections past it give way to "
     "peers on new hosts; a value set here or in p2p-parameters is a hard limit. 0 for no limit. Overrides maximum_connections_per_host in p2p-parameters.")
    ("p2p-host-limit-exempt", bpo::value<vector<string>>()->composing(), "An address or subnet (e.g. 172.17.0.1, 10.0.0.0/8, 2001:db8::/32) whose "
     "connections p2p-max-connections-per-host does not limit, such as a proxy that rewrites the source address of inbound peers. "
     "May be given more than once. Added to host_limit_exempt_subnets in p2p-parameters, which lists loopback by default.")
    ("p2p-parameters", bpo::value<string>(), ("P2P network parameters. (Default: " + fc::json::to_string(graphene::net::node_configuration()) + " )").c_str() )
    ;
  cli.add_options()
    ("p2p-force-validate", bpo::bool_switch()->default_value(false), "Force validation of all transactions." )
    ;
}

void p2p_plugin::plugin_initialize(const boost::program_options::variables_map& options)
{
  my = std::make_unique<detail::p2p_plugin_impl>(get_app().get_plugin<plugins::chain::chain_plugin>(), get_app());

  if( options.count( "p2p-endpoint" ) )
    my->endpoint = fc::ip::endpoint::from_string( options.at( "p2p-endpoint" ).as< string >() );

  my->user_agent = "Hive Reference Implementation";

  if( options.count( "p2p-max-connections" ) )
    my->max_connections = options.at( "p2p-max-connections" ).as< uint32_t >();

  if( options.count( "p2p-transaction-flush-delay-us" ) )
    my->transaction_flush_delay_us = options.at( "p2p-transaction-flush-delay-us" ).as< uint32_t >();

  if( options.count( "p2p-fetch-items-minimum-interval-us" ) )
    my->fetch_items_minimum_interval_us = options.at( "p2p-fetch-items-minimum-interval-us" ).as< uint32_t >();

  if( options.count( "p2p-max-transactions-in-flight" ) )
    my->max_transactions_in_flight = options.at( "p2p-max-transactions-in-flight" ).as< uint32_t >();

  if( options.count( "p2p-max-connections-per-host" ) )
    my->max_connections_per_host = options.at( "p2p-max-connections-per-host" ).as< uint32_t >();

  if( options.count( "p2p-host-limit-exempt" ) )
  {
    my->host_limit_exempt = options.at( "p2p-host-limit-exempt" ).as< vector< string > >();
    for( const string& subnet : my->host_limit_exempt )
      graphene::net::address_subnet::from_string( subnet ); // reject a malformed entry at startup
  }

  if (options.count("p2p-seed-node"))
  {
    vector<string> seeds;

    auto s = options.at("p2p-seed-node").as<vector<string>>();
    for (auto &arg : s)
    {
      vector<string> addresses;
      boost::split(addresses, arg, boost::is_any_of(" \t"));
      seeds.insert(seeds.end(), addresses.begin(), addresses.end());
    }

    for (const string &endpoint_string : seeds)
    {
      try
      {
        std::vector<fc::ip::endpoint> endpoints = fc::resolve_string_to_ip_endpoints(endpoint_string);
        my->seeds.insert(my->seeds.end(), endpoints.begin(), endpoints.end());
      }
      catch (const fc::exception &e)
      {
        // typically a DNS failure (no network yet, or a stale seed).  Keep the string and keep
        // trying after startup: a node started before its network was up used to stay peerless
        // forever, because this was the only place seeds were ever resolved
        wlog("caught exception ${e} while adding seed node ${endpoint}; will retry resolving it every ${secs} seconds once the P2P network is up",
             ("e", e.to_detail_string())("endpoint", endpoint_string)("secs", detail::HIVE_P2P_SEED_RESOLVE_RETRY_SECONDS));
        my->unresolved_seeds.push_back(endpoint_string);
      }
    }
  }

  my->force_validate = options.at( "p2p-force-validate" ).as< bool >();

  my->request_precomputing_transaction_signatures_if_useful();

  if( options.count("p2p-parameters") )
  {
    fc::variant var = fc::json::from_string( options.at("p2p-parameters").as<string>(), fc::json::format_validation_mode::full, fc::json::strict_parser );
    my->config = var.get_object();
  }
}

void p2p_plugin::plugin_startup()
{
//Everything was moved to `plugin_finalize_startup` to ensure that chain_plugin is fully started before p2p_plugin starts.
}

void p2p_plugin::plugin_finalize_startup()
{
  ilog("P2P plugin startup...");

  if( !my->chain.is_p2p_enabled() )
  {
    ilog("P2P plugin is not enabled...");
    return;
  }

  my->p2p_thread.async( [this]
  {
    my->node.reset(new graphene::net::node(my->user_agent, get_app(), my->chain.get_thread_pool()));
    my->node->load_configuration(get_app().data_dir() / "p2p");
    my->node->set_node_delegate( &(*my) );

    if( my->endpoint )
    {
      ilog("Configuring P2P to listen at ${ep}", ("ep", my->endpoint));
      my->node->listen_on_endpoint(*my->endpoint, true);
    }

    for( const auto& seed : my->seeds )
    {
      try
      {
        ilog("P2P adding seed node ${s}", ("s", seed));
        my->node->add_node(seed);
        //don't connect to seed nodes until we've started p2p network
        //my->node->connect_to_endpoint(seed);
      }
      catch( graphene::net::already_connected_to_requested_peer& )
      {
        wlog( "Already connected to seed node ${s}. Is it specified twice in config?", ("s", seed) );
      }
    }

    if( !my->unresolved_seeds.empty() )
    {
      ilog("${count} seed node(s) did not resolve during initialization, retrying every ${secs} seconds: ${seeds}",
           ("count", my->unresolved_seeds.size())("secs", detail::HIVE_P2P_SEED_RESOLVE_RETRY_SECONDS)("seeds", my->unresolved_seeds));
      my->retry_unresolved_seeds_task = fc::async( [this]() { my->retry_unresolved_seeds_loop(); }, "retry_unresolved_seeds" );
    }

    if( my->max_connections )
    {
      if( my->config.find( "maximum_number_of_connections" ) != my->config.end() )
        ilog( "Overriding advanded_node_parameters[ \"maximum_number_of_connections\" ] with ${cons}", ("cons", my->max_connections) );

      my->config.set( "maximum_number_of_connections", fc::variant( my->max_connections ) );
    }

    if( my->transaction_flush_delay_us )
    {
      if( my->config.find( "transaction_flush_delay_microseconds" ) != my->config.end() )
        ilog( "Overriding advanded_node_parameters[ \"transaction_flush_delay_microseconds\" ] with ${us}", ("us", *my->transaction_flush_delay_us) );

      my->config.set( "transaction_flush_delay_microseconds", fc::variant( *my->transaction_flush_delay_us ) );
    }

    if( my->fetch_items_minimum_interval_us )
    {
      if( my->config.find( "fetch_items_minimum_interval_microseconds" ) != my->config.end() )
        ilog( "Overriding advanded_node_parameters[ \"fetch_items_minimum_interval_microseconds\" ] with ${us}", ("us", *my->fetch_items_minimum_interval_us) );

      my->config.set( "fetch_items_minimum_interval_microseconds", fc::variant( *my->fetch_items_minimum_interval_us ) );
    }

    if( my->max_transactions_in_flight )
    {
      if( my->config.find( "maximum_transactions_in_flight" ) != my->config.end() )
        ilog( "Overriding advanded_node_parameters[ \"maximum_transactions_in_flight\" ] with ${count}", ("count", *my->max_transactions_in_flight) );

      my->config.set( "maximum_transactions_in_flight", fc::variant( *my->max_transactions_in_flight ) );
    }

    if( my->max_connections_per_host )
    {
      if( my->config.find( "maximum_connections_per_host" ) != my->config.end() )
        ilog( "Overriding advanded_node_parameters[ \"maximum_connections_per_host\" ] with ${count}", ("count", *my->max_connections_per_host) );

      my->config.set( "maximum_connections_per_host", fc::variant( *my->max_connections_per_host ) );
    }

    if( !my->host_limit_exempt.empty() )
    {
      vector< string > subnets = my->config.find( "host_limit_exempt_subnets" ) != my->config.end()
        ? my->config[ "host_limit_exempt_subnets" ].as< vector< string > >()
        : graphene::net::node_configuration().host_limit_exempt_subnets;
      subnets.insert( subnets.end(), my->host_limit_exempt.begin(), my->host_limit_exempt.end() );
      ilog( "Exempting ${subnets} from the per-host connection limit", ("subnets", subnets) );
      my->config.set( "host_limit_exempt_subnets", fc::variant( subnets ) );
    }

    ilog("Setting parameters");
    my->node->set_advanced_node_parameters( my->config );

    ilog("Listening to P2P network");
    my->node->listen_to_p2p_network( [this](){ return get_app().is_interrupt_request(); } );
    if( get_app().is_interrupt_request() )
    {
      ilog("P2P plugin was manually closed. More details in p2p log.");
      return;
    }

    ilog("Connection to P2P network");
    my->node->connect_to_p2p_network();
    ilog("Connected to P2P network");

    block_id_type block_id;
    my->chain.db().with_read_lock( [&]()
    {
      block_id = my->chain.db().head_block_id();
    });
    my->node->sync_from(graphene::net::item_id(graphene::net::block_message_type, block_id), std::vector<uint32_t>());
    ilog("P2P node listening at ${ep}", ("ep", my->node->get_actual_listening_endpoint()));
    get_app().notify_webserver(
        "P2P",
        static_cast<fc::string>(my->node->get_actual_listening_endpoint().get_address()),
        my->node->get_actual_listening_endpoint().port()
    );
  }).wait();
  ilog( "P2P Plugin started" );
  get_app().notify_status("P2P started");
}

void p2p_plugin::plugin_pre_shutdown() {

  ilog("Shutting down P2P Plugin...");

  if( !my->chain.is_p2p_enabled() )
  {
    ilog("P2P plugin is not enabled...");
    return;
  }

  try
  {
    my->shutdown_helper.shutdown();
  }
  catch (const fc::exception& e)
  {
    // if shutdown fails, maybe because there are unprocessed blocks in the 
    // write queue and the write thread didn't finish processing them before
    // the timeout expired, swallow the exception and try to continue
    // shutting down the p2p plugin.  There's a small chance this will cause
    // a crash, but if we let the exception leak then we're guaranteed to 
    // exit uncleanly.
    elog("P2P shutdown timed out before all blocks/transactions were processed: ${e}", (e));
  }

  ilog("P2P Plugin: terminating p2p tasks");
  if( my->retry_unresolved_seeds_task.valid() )
  {
    // cancel from the p2p thread, the way node_impl::close() cancels the node's own loops
    my->p2p_thread.async( [this]()
    {
      try
      {
        my->retry_unresolved_seeds_task.cancel_and_wait("p2p_plugin::plugin_pre_shutdown()");
      }
      catch (const fc::canceled_exception&)
      {
      }
      catch (const fc::exception& e)
      {
        wlog("Exception thrown while terminating retry_unresolved_seeds task, ignoring: ${e}", (e));
      }
    }, "cancel_retry_unresolved_seeds" ).wait();
  }
  if( my->node )
  {
    my->node->close();
    ilog("Waiting for p2p_thread quit");
    my->p2p_thread.quit();
    ilog("p2p_thread quit done");
    my->node.reset();
  }
  else
  {
    ilog("P2P node was not fully initialized, skipping close");
    my->p2p_thread.quit();
  }
}

void p2p_plugin::plugin_shutdown()
{
  //Nothing to do. Everything is closed/finished during `pre_shutdown` stage,
}

void p2p_plugin::broadcast_block(const std::shared_ptr<hive::chain::full_block_type>& full_block)
{
  if( !my->node )
    return;

  uint32_t block_num = full_block->get_block_num();
  size_t transaction_count = full_block->get_full_transactions().size();
  ulog("Broadcasting block #${block_num} with ${transaction_count} transactions", (block_num)(transaction_count));
  my->node->broadcast(full_block);
}

void p2p_plugin::broadcast_transaction(const std::shared_ptr<hive::chain::full_transaction_type>& full_transaction)
{
  if( !my->node )
    return;

  ulog("Broadcasting tx #${id}", ("id", full_transaction->get_transaction_id()));
  my->node->broadcast(full_transaction);
}

void p2p_plugin::set_block_production( bool producing_blocks )
{
  my->block_producer = producing_blocks;
  my->request_precomputing_transaction_signatures_if_useful();
}

fc::variant_object p2p_plugin::get_info()
{
  if( !my->node )
    return fc::variant_object();

  fc::mutable_variant_object result = my->node->network_get_info();
  result["connection_count"] = my->node->get_connection_count();
  return result;
}

graphene::net::node_configuration p2p_plugin::get_advanced_node_parameters()
{
  FC_ASSERT( my->node, "p2p node is not running" );
  return my->node->get_advanced_node_parameters();
}

void p2p_plugin::add_node(const fc::ip::endpoint& endpoint)
{
  if( !my->node )
    return;

  my->node->add_node(endpoint);
}

void p2p_plugin::set_allowed_peers(const std::vector<graphene::net::node_id_t>& allowed_peers)
{
  if( !my->node )
    return;

  my->node->set_allowed_peers(allowed_peers);
}

std::vector< api_peer_status > p2p_plugin::get_connected_peers()
{
  if( !my->node )
    return {};

  std::vector<graphene::net::peer_status> connected_peers = my->node->get_connected_peers();
  std::vector<api_peer_status> api_connected_peers;
  api_connected_peers.reserve( connected_peers.size() );
  for (const graphene::net::peer_status& peer : connected_peers)
    api_connected_peers.emplace_back(peer.version, peer.host, peer.info);
  return api_connected_peers;
}

} } } // namespace hive::plugins::p2p
