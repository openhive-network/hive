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

#include <boost/any.hpp>

#include <chrono>

using std::string;
using std::vector;

#define HIVE_P2P_NUMBER_THREAD_SENSITIVE_ACTIONS 2
#define HIVE_P2P_BLOCK_HANDLER 0
#define HIVE_P2P_TRANSACTION_HANDLER 1

namespace hive { namespace plugins { namespace p2p {

using appbase::app;

using graphene::net::item_hash_t;
using graphene::net::item_id;
using graphene::net::message;
using graphene::net::block_message;
using graphene::net::trx_message;

using hive::protocol::block_header;
using hive::protocol::block_id_type;

namespace detail {

// This exists in p2p_plugin and http_plugin. It should be added to fc.
std::vector<fc::ip::endpoint> resolve_string_to_ip_endpoints( const std::string& endpoint_string )
{
  try
  {
    string::size_type colon_pos = endpoint_string.find( ':' );
    if( colon_pos == std::string::npos )
      FC_THROW( "Missing required port number in endpoint string \"${endpoint_string}\"", ("endpoint_string", endpoint_string) );

    std::string port_string = endpoint_string.substr( colon_pos + 1 );

    try
    {
      uint16_t port = boost::lexical_cast< uint16_t >( port_string );
      std::string hostname = endpoint_string.substr( 0, colon_pos );
      std::vector< fc::ip::endpoint > endpoints = fc::resolve( hostname, port );

      if( endpoints.empty() )
        FC_THROW_EXCEPTION( fc::unknown_host_exception, "The host name can not be resolved: ${hostname}", ("hostname", hostname) );

      return endpoints;
    }
    catch( const boost::bad_lexical_cast& )
    {
      FC_THROW("Bad port: ${port}", ("port", port_string) );
    }
  }
  FC_CAPTURE_AND_RETHROW( (endpoint_string) )
}

class p2p_plugin_impl : public graphene::net::node_delegate
{
public:

  p2p_plugin_impl( plugins::chain::chain_plugin& c )
    : shutdown_helper( "P2P plugin", HIVE_P2P_NUMBER_THREAD_SENSITIVE_ACTIONS, { "HIVE_P2P_BLOCK_HANDLER", "HIVE_P2P_TRANSACTION_HANDLER" } ), chain( c )
  {
  }
  virtual ~p2p_plugin_impl()
  {
    ilog("P2P plugin is closing...");
    shutdown_helper.shutdown();
    ilog("P2P plugin was closed...");
    hive::notify_hived_status("P2P stopped");
  }

  bool is_included_block(const block_id_type& block_id);
  virtual hive::protocol::chain_id_type get_old_chain_id() const override;
  virtual hive::protocol::chain_id_type get_new_chain_id() const override;
  virtual hive::protocol::chain_id_type get_chain_id() const override;

  // node_delegate interface
  virtual bool has_item( const graphene::net::item_id& ) override;
  virtual bool handle_block(const std::shared_ptr<hive::chain::full_block_type>& full_block, bool) override;
  virtual void handle_transaction(const std::shared_ptr<hive::chain::full_transaction_type>& full_transaction) override;
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
  virtual uint32_t estimate_last_known_fork_from_git_revision_timestamp( uint32_t ) const override;
  virtual void error_encountered( const std::string& message, const fc::oexception& error ) override;
  virtual std::deque<block_id_type>::const_iterator find_first_item_not_in_blockchain(const std::deque<block_id_type>& item_hashes_received) override;

  void request_precomputing_transaction_signatures_if_useful();

  fc::optional<fc::ip::endpoint> endpoint;
  vector<fc::ip::endpoint> seeds;
  string user_agent;
  fc::mutable_variant_object config;
  uint32_t max_connections = 0;
  bool force_validate = false;
  bool block_producer = false;

  shutdown_mgr shutdown_helper;

  std::unique_ptr<graphene::net::node> node;

  plugins::chain::chain_plugin& chain;

  fc::thread p2p_thread;
};

////////////////////////////// Begin node_delegate Implementation //////////////////////////////
bool p2p_plugin_impl::has_item( const graphene::net::item_id& id )
{
  try
  {
    if( id.item_type == graphene::net::block_message_type )
      return chain.db().is_known_block(id.item_hash);
    else
      return chain.db().with_read_lock( [&]() {
        return chain.db().is_known_transaction(id.item_hash);
      });
  }
  FC_CAPTURE_LOG_AND_RETHROW( (id) )
}

bool p2p_plugin_impl::handle_block(const std::shared_ptr<hive::chain::full_block_type>& full_block, bool sync_mode)
{ try {
  if( shutdown_helper.get_running().load() )
  {
    action_catcher ac( shutdown_helper.get_running(), shutdown_helper.get_state( HIVE_P2P_BLOCK_HANDLER ) );

    uint32_t head_block_num = chain.db().head_block_num_from_fork_db();
    if (sync_mode)
      fc_ilog(fc::logger::get("sync"),
          "chain pushing sync block #${block_num} ${block_hash}, head is ${head}",
          ("block_num", full_block->get_block_num())
          ("block_hash", full_block->get_block_id())
          ("head", head_block_num));
    else
      fc_ilog(fc::logger::get("sync"),
          "chain pushing block #${block_num} ${block_hash}, head is ${head}",
          ("block_num", full_block->get_block_num())
          ("block_hash", full_block->get_block_id())
          ("head", head_block_num));

    try
    {
      // TODO: in the case where this block is valid but on a fork that's too old for us to switch to,
      // you can help the network code out by throwing a block_older_than_undo_history exception.
      // when the net code sees that, it will stop trying to push blocks from that chain, but
      // leave that peer connected so that they can get sync blocks from us
      bool result = chain.accept_block(full_block, sync_mode, (block_producer | force_validate) ? chain::database::skip_nothing : chain::database::skip_transaction_signatures, chain::chain_plugin::lock_type::fc);

      if (!sync_mode)
      {
        fc::microseconds offset = fc::time_point::now() - full_block->get_block_header().timestamp;
        STATSD_TIMER("p2p", "offset", "block_arrival", offset, 1.0f)
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
      fc_elog(fc::logger::get("sync"), "Error when pushing block, current head block is ${head}:\n${e}",
              ("e", e.to_detail_string())
              ("head", head_block_num));
      elog("Error when pushing block:\n${e}", ("e", e.to_detail_string()));
      FC_THROW_EXCEPTION(graphene::net::unlinkable_block_exception, "Error when pushing block:\n${e}", ("e", e.to_detail_string()));
    }
    catch (const fc::exception& e)
    {
      //fc_elog(fc::logger::get("sync"), "Error when pushing block, current head block is ${head}:\n${e}", ("e", e.to_detail_string()) ("head", head_block_num));
      //elog("Error when pushing block:\n${e}", ("e", e.to_detail_string()));
      if (e.code() == 4080000)
      {
        elog("Rethrowing as graphene::net exception");
        FC_THROW_EXCEPTION(graphene::net::unlinkable_block_exception, "Error when pushing block:\n${e}", ("e", e.to_detail_string()));
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
  return false;
} FC_CAPTURE_AND_RETHROW((sync_mode)) }

void p2p_plugin_impl::handle_transaction(const std::shared_ptr<hive::chain::full_transaction_type>& full_transaction)
{
  if (shutdown_helper.get_running().load())
  {
    try
    {
      action_catcher ac(shutdown_helper.get_running(), shutdown_helper.get_state(HIVE_P2P_TRANSACTION_HANDLER));

      chain.accept_transaction(full_transaction, chain::chain_plugin::lock_type::fc);

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
    return chain.db().get_block_ids(blockchain_synopsis, remaining_item_count, limit);
  }
  catch (const hive::chain::internal_peer_is_on_an_unreachable_fork&)
  {
    FC_THROW_EXCEPTION(graphene::net::peer_is_on_an_unreachable_fork, "Unable to provide a list of blocks starting at any of the blocks in peer's synopsis");
  }
} FC_CAPTURE_AND_RETHROW((blockchain_synopsis)(remaining_item_count)(limit)) }

std::shared_ptr<chain::full_block_type> p2p_plugin_impl::get_full_block(const block_id_type& id)
{ try {
  fc_dlog(fc::logger::get("chainlock"), "get_full_block will get forkdb read lock");
  return chain.db().fetch_block_by_id(id);
} FC_CAPTURE_AND_RETHROW((id)) }

hive::protocol::chain_id_type p2p_plugin_impl::get_old_chain_id() const
{
  return chain.db().get_old_chain_id();
}

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
  return chain.db().get_blockchain_synopsis(reference_point, number_of_blocks_after_reference_point);
} FC_LOG_AND_RETHROW() }

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
    std::shared_ptr<hive::chain::full_block_type> block = chain.db().fetch_block_by_id(block_id);
    return block ? block->get_block_header().timestamp : fc::time_point_sec::min();
  } FC_CAPTURE_AND_RETHROW((block_id))
}

graphene::net::item_hash_t p2p_plugin_impl::get_head_block_id() const
{ try {
  return chain.db().head_block_id_from_fork_db();
} FC_CAPTURE_AND_RETHROW() }

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
  return chain.db().find_first_item_not_in_blockchain(item_hashes_received);
}

void p2p_plugin_impl::request_precomputing_transaction_signatures_if_useful()
{
  if (force_validate || block_producer)
    chain::blockchain_worker_thread_pool::get_instance().set_p2p_force_validate();
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
    block_id_type block_id_in_preferred_chain = chain.db().get_block_id_for_num(block_num);
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
    ("seed-node", bpo::value<vector<string>>()->composing(), "The IP address and port of a remote peer to sync with. Deprecated in favor of p2p-seed-node.")
    ("p2p-seed-node", bpo::value<vector<string>>()->composing()->default_value( default_seeds, seed_ss.str() ), "The IP address and port of a remote peer to sync with.")
    ("p2p-parameters", bpo::value<string>(), ("P2P network parameters. (Default: " + fc::json::to_string(graphene::net::node_configuration()) + " )").c_str() )
    ;
  cli.add_options()
    ("force-validate", bpo::bool_switch()->default_value(false), "Force validation of all transactions. Deprecated in favor of p2p-force-validate" )
    ("p2p-force-validate", bpo::bool_switch()->default_value(false), "Force validation of all transactions." )
    ;
}

void p2p_plugin::plugin_initialize(const boost::program_options::variables_map& options)
{
  my = std::make_unique<detail::p2p_plugin_impl>(appbase::app().get_plugin<plugins::chain::chain_plugin>());

  if( options.count( "p2p-endpoint" ) )
    my->endpoint = fc::ip::endpoint::from_string( options.at( "p2p-endpoint" ).as< string >() );

  my->user_agent = "Hive Reference Implementation";

  if( options.count( "p2p-max-connections" ) )
    my->max_connections = options.at( "p2p-max-connections" ).as< uint32_t >();

  if( options.count( "seed-node" ) || options.count( "p2p-seed-node" ) )
  {
    vector< string > seeds;
    if( options.count( "seed-node" ) )
    {
      wlog( "Option seed-node is deprecated in favor of p2p-seed-node" );
      auto s = options.at("seed-node").as<vector<string>>();
      for( auto& arg : s )
      {
        vector< string > addresses;
        boost::split( addresses, arg, boost::is_any_of( " \t" ) );
        seeds.insert( seeds.end(), addresses.begin(), addresses.end() );
      }
    }

    if( options.count( "p2p-seed-node" ) )
    {
      auto s = options.at("p2p-seed-node").as<vector<string>>();
      for( auto& arg : s )
      {
        vector< string > addresses;
        boost::split( addresses, arg, boost::is_any_of( " \t" ) );
        seeds.insert( seeds.end(), addresses.begin(), addresses.end() );
      }
    }

    for( const string& endpoint_string : seeds )
    {
      try
      {
        std::vector<fc::ip::endpoint> endpoints = detail::resolve_string_to_ip_endpoints(endpoint_string);
        my->seeds.insert( my->seeds.end(), endpoints.begin(), endpoints.end() );
      }
      catch( const fc::exception& e )
      {
        wlog( "caught exception ${e} while adding seed node ${endpoint}",
          ("e", e.to_detail_string())("endpoint", endpoint_string) );
      }
    }
  }

  my->force_validate = options.at( "p2p-force-validate" ).as< bool >();

  if( !my->force_validate && options.at( "force-validate" ).as< bool >() )
  {
    wlog( "Option force-validate is deprecated in favor of p2p-force-validate" );
    my->force_validate = true;
  }
  my->request_precomputing_transaction_signatures_if_useful();

  if( options.count("p2p-parameters") )
  {
    fc::variant var = fc::json::from_string( options.at("p2p-parameters").as<string>(), fc::json::strict_parser );
    my->config = var.get_object();
  }
}

void p2p_plugin::plugin_startup()
{
  ilog("P2P plugin startup...");

  if( !my->chain.is_p2p_enabled() )
  {
    ilog("P2P plugin is not enabled...");
    return;
  }

  my->p2p_thread.async( [this]
  {
    my->node.reset(new graphene::net::node(my->user_agent));
    my->node->load_configuration(app().data_dir() / "p2p");
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

    if( my->max_connections )
    {
      if( my->config.find( "maximum_number_of_connections" ) != my->config.end() )
        ilog( "Overriding advanded_node_parameters[ \"maximum_number_of_connections\" ] with ${cons}", ("cons", my->max_connections) );

      my->config.set( "maximum_number_of_connections", fc::variant( my->max_connections ) );
    }

    ilog("Setting parameters");
    my->node->set_advanced_node_parameters( my->config );

    ilog("Listening to P2P network");
    my->node->listen_to_p2p_network( [](){ return appbase::app().is_interrupt_request(); } );
    if( appbase::app().is_interrupt_request() )
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
    hive::notify( "P2P listening",
    // {
        "type", "p2p",
        "address", static_cast<fc::string>(my->node->get_actual_listening_endpoint().get_address()),
        "port", my->node->get_actual_listening_endpoint().port()
    // }
    );
  }).wait();
  ilog( "P2P Plugin started" );
  hive::notify_hived_status("P2P started");
}

void p2p_plugin::plugin_pre_shutdown() {

  ilog("Shutting down P2P Plugin...");

  if( !my->chain.is_p2p_enabled() )
  {
    ilog("P2P plugin is not enabled...");
    return;
  }

  my->shutdown_helper.shutdown();

  ilog("P2P Plugin: checking handle_block and handle_transaction activity");
  my->node->close();
  fc::promise<void>::ptr quitDone(new fc::promise<void>("P2P thread quit"));
  my->p2p_thread.quit(quitDone.get());
  ilog("Waiting for p2p_thread quit");
  quitDone->wait();
  ilog("p2p_thread quit done");
  my->node.reset();
}

void p2p_plugin::plugin_shutdown()
{
  //Nothing to do. Everything is closed/finished during `pre_shutdown` stage,
}

void p2p_plugin::broadcast_block(const std::shared_ptr<hive::chain::full_block_type>& full_block)
{
  uint32_t block_num = full_block->get_block_num();
  size_t transction_count = full_block->get_full_transactions().size();
  ulog("Broadcasting block #${block_num} with ${transction_count} transactions", (block_num)(transction_count));
  my->node->broadcast(full_block);
}

void p2p_plugin::broadcast_transaction(const std::shared_ptr<hive::chain::full_transaction_type>& full_transaction)
{
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
  fc::mutable_variant_object result = my->node->network_get_info();
  result["connection_count"] = my->node->get_connection_count();
  return result;
}

void p2p_plugin::add_node(const fc::ip::endpoint& endpoint)
{
  my->node->add_node(endpoint);
}

void p2p_plugin::set_allowed_peers(const std::vector<graphene::net::node_id_t>& allowed_peers)
{
  my->node->set_allowed_peers(allowed_peers);
}

std::vector< api_peer_status > p2p_plugin::get_connected_peers()
{
  std::vector<graphene::net::peer_status> connected_peers = my->node->get_connected_peers();
  std::vector<api_peer_status> api_connected_peers;
  api_connected_peers.reserve( connected_peers.size() );
  for (const graphene::net::peer_status& peer : connected_peers)
    api_connected_peers.emplace_back(peer.version, peer.host, peer.info);
  return api_connected_peers;
}

} } } // namespace hive::plugins::p2p
