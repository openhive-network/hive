#include <hive/plugins/account_by_key_api/account_by_key_api_plugin.hpp>
#include <hive/plugins/account_by_key_api/account_by_key_api.hpp>
#include <hive/plugins/account_history_api/account_history_api_plugin.hpp>
#include <hive/plugins/account_history_api/account_history_api.hpp>
#include <hive/plugins/database_api/database_api.hpp>
#include <hive/plugins/database_api/database_api_plugin.hpp>
#include <hive/plugins/block_api/block_api.hpp>
#include <hive/plugins/block_api/block_api_plugin.hpp>
#include <hive/plugins/market_history_api/market_history_api_plugin.hpp>
#include <hive/plugins/market_history_api/market_history_api.hpp>
#include <hive/plugins/network_broadcast_api/network_broadcast_api_plugin.hpp>
#include <hive/plugins/network_broadcast_api/network_broadcast_api.hpp>
#include <hive/plugins/p2p/p2p_plugin.hpp>
#include <hive/plugins/wallet_bridge_api/wallet_bridge_api.hpp>
#include <hive/plugins/wallet_bridge_api/wallet_bridge_api_plugin.hpp>
#include <hive/plugins/rc_api/rc_api.hpp>
#include <hive/plugins/rc_api/rc_api_plugin.hpp>

namespace hive { namespace plugins { namespace wallet_bridge_api {

typedef std::function< void( const broadcast_transaction_synchronous_return& ) > confirmation_callback;

class wallet_bridge_api_impl
{
  public:
    wallet_bridge_api_impl();
    ~wallet_bridge_api_impl();

    void on_post_apply_block( const protocol::signed_block& b );

    DECLARE_API_IMPL(
        (get_version)
        (get_witness_schedule)
        (get_current_median_history_price)
        (get_hardfork_version)
        (get_block)
        (get_chain_properties)
        (get_ops_in_block)
        (get_feed_history)
        (get_active_witnesses)
        (get_withdraw_routes)
        (list_my_accounts)
        (list_accounts)
        (get_dynamic_global_properties)
        (get_account)
        (get_accounts)
        (get_transaction)
        (list_witnesses)
        (get_witness)
        (get_conversion_requests)
        (get_collateralized_conversion_requests)
        (get_order_book)
        (get_open_orders)
        (get_owner_history)
        (get_account_history)
        (list_proposals)
        (find_proposals)
        (is_known_transaction)
        (list_proposal_votes)
        (get_reward_fund)
        (broadcast_transaction_synchronous)
        (broadcast_transaction)
        (find_recurrent_transfers)
        (find_rc_accounts)
        (list_rc_accounts)
        (list_rc_direct_delegations)
    )

    chain::chain_plugin&                                            _chain;
    chain::database&                                                _db;
    std::shared_ptr< account_by_key::account_by_key_api >           _account_by_key_api;
    std::shared_ptr< account_history::account_history_api >         _account_history_api;
    std::shared_ptr< database_api::database_api >                   _database_api;
    std::shared_ptr< block_api::block_api >                         _block_api;
    std::shared_ptr< market_history::market_history_api >           _market_history_api;
    std::shared_ptr< network_broadcast_api::network_broadcast_api>  _network_broadcast_api;
    std::shared_ptr< rc::rc_api>                                    _rc_api;
    p2p::p2p_plugin*                                                _p2p = nullptr;
    map< protocol::transaction_id_type, confirmation_callback >     _callbacks;
    map< time_point_sec, vector< protocol::transaction_id_type > >  _callback_expirations;
    boost::signals2::connection                                     _on_post_apply_block_conn;
    boost::mutex                                                    _mtx;

  private:

    protocol::signed_transaction get_trx( const variant& args );

    /**
    * @brief Requires the variant to store an array of length at least `length_required`
    */
    void verify_args( const variant& v, size_t length_required )const;
};

/* CONSTRUCTORS AND DESTRUCTORS */

wallet_bridge_api::wallet_bridge_api()
  : my( new wallet_bridge_api_impl() )
{
  JSON_RPC_REGISTER_API( HIVE_WALLET_BRIDGE_API_PLUGIN_NAME );
}

wallet_bridge_api::~wallet_bridge_api() {}

void wallet_bridge_api::api_startup()
{
  std::string not_enabled_plugins;

  //=====
  auto db_api = appbase::app().find_plugin< database_api::database_api_plugin >();
  if (db_api)
    my->_database_api = db_api->api;
  else
    not_enabled_plugins += "database_api ";

  //=====
  auto block_api = appbase::app().find_plugin< block_api::block_api_plugin >();
  if (block_api)
    my->_block_api = block_api->api;
  else
    not_enabled_plugins += "block_api ";

  //=====
  auto account_history_api = appbase::app().find_plugin< account_history::account_history_api_plugin >();
  if (account_history_api)
    my->_account_history_api = account_history_api->api;
  else
    not_enabled_plugins += "account_history_api ";

  //=====
  auto account_by_key_api = appbase::app().find_plugin< account_by_key::account_by_key_api_plugin >();
  if (account_by_key_api)
    my->_account_by_key_api = account_by_key_api->api;
  else
    not_enabled_plugins += "account_by_key_api ";

  //=====
  auto market_history_api = appbase::app().find_plugin< market_history::market_history_api_plugin >();
  if (market_history_api)
    my->_market_history_api = market_history_api->api;
  else
    not_enabled_plugins += "market_history_api ";

  //=====
  auto network_broadcast_api = appbase::app().find_plugin< network_broadcast_api::network_broadcast_api_plugin >();
  if (network_broadcast_api)
    my->_network_broadcast_api = network_broadcast_api->api;
  else
    not_enabled_plugins += "network_broadcast_api ";

  //=====
  auto rc_api = appbase::app().find_plugin< rc::rc_api_plugin >();
  if (rc_api)
    my->_rc_api = rc_api->api;
  else
    not_enabled_plugins += "rc_api_plugin";

  //=====
  auto p2p = appbase::app().find_plugin< p2p::p2p_plugin >();
  if (p2p)
    my->_p2p = p2p;
  else
    not_enabled_plugins += "p2p_plugin";

  if (not_enabled_plugins.empty())
    ilog("Wallet bridge api initialized");
  else
    ilog("Wallet bridge api initialized. Missing plugins: ${missing_plugins}", ( "missing_plugins", not_enabled_plugins ));
}

wallet_bridge_api_impl::wallet_bridge_api_impl() : 
  _chain(appbase::app().get_plugin< hive::plugins::chain::chain_plugin >()),
  _db( _chain.db() )
  {
    _on_post_apply_block_conn = _db.add_post_apply_block_handler([&]( const chain::block_notification& note ){ on_post_apply_block( note.block ); },
    appbase::app().get_plugin< hive::plugins::wallet_bridge_api::wallet_bridge_api_plugin >(), 0 );
  }

wallet_bridge_api_impl::~wallet_bridge_api_impl() {}

void wallet_bridge_api_impl::verify_args( const variant& v, size_t length_required )const
{
  FC_ASSERT( v.get_type() == variant::array_type, "Variant must store an array type" );
  FC_ASSERT( v.get_array().size() >= length_required, "Array is of invalid size: ${size}. Required: ${req}",
    ("size", v.get_array().size())("req",length_required) );
}

void wallet_bridge_api_impl::on_post_apply_block( const protocol::signed_block& b )
  { try {
    boost::lock_guard< boost::mutex > guard( _mtx );
    int32_t block_num = int32_t(b.block_num());
    if( _callbacks.size() )
    {
      for( size_t trx_num = 0; trx_num < b.transactions.size(); ++trx_num )
      {
        const auto& trx = b.transactions[trx_num];
        auto id = trx.id();
        auto itr = _callbacks.find( id );
        if( itr == _callbacks.end() ) continue;
        itr->second( broadcast_transaction_synchronous_return( {id, block_num, int32_t( trx_num ), false }) );
        _callbacks.erase( itr );
      }
    }

    /// clear all expirations
    while( true )
    {
      auto exp_it = _callback_expirations.begin();
      if( exp_it == _callback_expirations.end() )
        break;
      if( exp_it->first >= b.timestamp )
        break;
      for( const protocol::transaction_id_type& txid : exp_it->second )
      {
        auto cb_it = _callbacks.find( txid );
        // If it's empty, that means the transaction has been confirmed and has been deleted by the above check.
        if( cb_it == _callbacks.end() )
          continue;

        confirmation_callback callback = cb_it->second;
        protocol::transaction_id_type txid_byval = txid;    // can't pass in by reference as it's going to be deleted
        callback( broadcast_transaction_synchronous_return( {txid_byval, block_num, -1, true }) );

        _callbacks.erase( cb_it );
      }
      _callback_expirations.erase( exp_it );
    }
  } FC_LOG_AND_RETHROW()
}

/* API IMPLEMENTATION
  Arguments to every method are passed via variant.
  Because DECLARE_API adds "bool lock = false" argument to every method, every variant contains at least bool.
  If method needs any other arguments, then variant is 2 elements array. First is argument/array with arguments for method, second is bool.
*/

DEFINE_API_IMPL( wallet_bridge_api_impl, get_version )
{
  FC_ASSERT( _database_api, "database_api_plugin not enabled." );
  return _database_api->get_version({});
}

DEFINE_API_IMPL( wallet_bridge_api_impl, get_block )
{
  FC_ASSERT(_block_api , "block_api_plugin not enabled." );
  verify_args( args, 1 );
  FC_ASSERT( args.get_array()[0].is_numeric(), "block number is required as first argument" );
  const uint32_t block_num = args.get_array()[0].as<uint32_t>();
  get_block_return result;
  result.block = _block_api->get_block({block_num}).block;
  return result;
}

DEFINE_API_IMPL( wallet_bridge_api_impl, get_chain_properties )
{
  FC_ASSERT( _database_api, "database_api_plugin not enabled." );
  return _database_api->get_witness_schedule( {} ).median_props;
}

DEFINE_API_IMPL( wallet_bridge_api_impl, get_witness_schedule )
{
  FC_ASSERT(_database_api , "database_api_plugin not enabled." );
  return _database_api->get_witness_schedule({});
}

DEFINE_API_IMPL( wallet_bridge_api_impl, get_current_median_history_price )
{
  FC_ASSERT(_database_api , "database_api_plugin not enabled." );
  return _database_api->get_current_price_feed( {} );;
}

DEFINE_API_IMPL( wallet_bridge_api_impl, get_hardfork_version )
{
  FC_ASSERT(_database_api , "database_api_plugin not enabled." );
  return _database_api->get_hardfork_properties( {} ).current_hardfork_version;
}

DEFINE_API_IMPL( wallet_bridge_api_impl, get_ops_in_block )
{
  FC_ASSERT( _account_history_api, "account_history_api_plugin not enabled." );
  verify_args( args, 1 );
  FC_ASSERT( args.get_array()[0].is_array(), "get_ops_in_block needs at least one argument" );
  const auto arguments = args.get_array()[0];
  verify_args( arguments, 2 );
  FC_ASSERT( arguments.get_array()[0].is_numeric(), "block number is required as first argument" );
  FC_ASSERT( arguments.get_array()[1].is_bool(), "only virtual operation (bool) is required as second argument" );

  const uint32_t block_num = arguments.get_array()[0].as<uint32_t>();
  const bool only_virtual = arguments.get_array()[1].as_bool();

  get_ops_in_block_return result;
  result.ops = _account_history_api->get_ops_in_block( { block_num, only_virtual } ).ops;
  return result;
}

DEFINE_API_IMPL( wallet_bridge_api_impl, get_feed_history )
{
  FC_ASSERT( _database_api, "database_api_plugin not enabled." );
  return _database_api->get_feed_history({});
}

DEFINE_API_IMPL( wallet_bridge_api_impl, get_active_witnesses )
{
  FC_ASSERT( _database_api, "database_api_plugin not enabled." );
  get_active_witnesses_return result;
  result.witnesses = _database_api->get_active_witnesses( {} ).witnesses;
  return result;
}

DEFINE_API_IMPL( wallet_bridge_api_impl, get_withdraw_routes )
{
  FC_ASSERT( _database_api, "database_api_plugin not enabled." );
  verify_args( args, 1 );
  FC_ASSERT( args.get_array()[0].is_array(), "get_withdraw_routes needs at least one argument" );
  const auto arguments = args.get_array()[0];
  verify_args( arguments, 2 );
  FC_ASSERT( arguments.get_array()[0].is_string(), "Account name is required as first argument" );
  FC_ASSERT( arguments.get_array()[1].is_string(), "Withdraw route type is required as second argument" );

  const protocol::account_name_type account = arguments.get_array()[0].get_string();
  const auto route = arguments.get_array()[1].as<database_api::withdraw_route_type>();

  get_withdraw_routes_return result;

  if( route == database_api::withdraw_route_type::outgoing || route == database_api::withdraw_route_type::all )
    result = _database_api->find_withdraw_vesting_routes({ account, database_api::by_withdraw_route });

  if( route == database_api::withdraw_route_type::incoming || route == database_api::withdraw_route_type::all )
  {
    get_withdraw_routes_return _result = _database_api->find_withdraw_vesting_routes({ account, database_api::by_destination });
    std::move( _result.routes.begin(), _result.routes.end(), std::back_inserter( result.routes ) );
  }

  return result;
}

DEFINE_API_IMPL( wallet_bridge_api_impl, list_my_accounts )
{
  FC_ASSERT( _account_by_key_api, "account_by_key_api_plugin not enabled." );
  verify_args( args, 1 );
  FC_ASSERT( args.get_array()[0].is_array(), "Array of keys is required as first argument" );
  const auto arg_keys = args.get_array()[0].get_array();
  vector<protocol::public_key_type> keys;
  keys.reserve(arg_keys.size());
  for (const auto& arg : arg_keys)
    keys.push_back(arg.as<protocol::public_key_type>());

  return _account_by_key_api->get_key_references( {keys} );
}

DEFINE_API_IMPL( wallet_bridge_api_impl, list_accounts )
{
  verify_args( args, 1 );
  FC_ASSERT(args.get_array()[0].is_array(), "list_accounts needs at least one argument");
  const auto arguments = args.get_array()[0];
  verify_args( arguments, 2 );
  FC_ASSERT( arguments.get_array()[0].is_string(), "Lowerbound account name is required as first argument" );
  FC_ASSERT( arguments.get_array()[1].is_numeric(), "Accounts limit is required as second argument" );
  const protocol::account_name_type lowerbound = arguments.get_array()[0].get_string();
  uint32_t limit = arguments.get_array()[1].as<uint32_t>();
  FC_ASSERT( limit <= 1000 );
  const auto& accounts_by_name = _db.get_index< chain::account_index, chain::by_name >();
  list_accounts_return result;

  for( auto itr = accounts_by_name.lower_bound( lowerbound ); limit-- && itr != accounts_by_name.end(); ++itr )
    result.emplace( itr->name );

  return result;
}

DEFINE_API_IMPL( wallet_bridge_api_impl, get_dynamic_global_properties )
{
  FC_ASSERT( _database_api, "database_api_plugin not enabled." );
  return _database_api->get_dynamic_global_properties( {} );
}

DEFINE_API_IMPL( wallet_bridge_api_impl, get_account )
{
  verify_args( args, 1 );
  FC_ASSERT( args.get_array()[0].is_string(), "account name is required as first argument" );
  chain::account_name_type acc_name = args.get_array()[0].get_string();
  const chain::account_object* account = _db.find_account(acc_name);
  get_account_return result;
  if (account)
    result = database_api::api_account_object(*account, _db, true);

  return result;
}

DEFINE_API_IMPL( wallet_bridge_api_impl, get_accounts )
{
  verify_args( args, 1 );
  FC_ASSERT( args.get_array()[0].is_array(), "Array of account names is required as first argument" );
  const auto account_names = args.get_array()[0].get_array();
  const bool delayed_votes_active = true;
  get_accounts_return result;
  result.reserve(account_names.size());

  for( const auto& acc_name: account_names )
  {
    FC_ASSERT(acc_name.is_string(), "Array contains object which is not an account name");
    const chain::account_object* account = _db.find_account(acc_name.get_string());
    if (account)
      result.push_back(database_api::api_account_object( *account, _db, delayed_votes_active ) );
  }

  result.shrink_to_fit();
  return result;
}

DEFINE_API_IMPL( wallet_bridge_api_impl, get_transaction )
{
  FC_ASSERT( _account_history_api, "account_history_api_plugin not enabled." );
  verify_args( args, 1 );
  FC_ASSERT( args.get_array()[0].is_string(), "Transaction id is required as second argument" );
  const protocol::transaction_id_type id = args.get_array()[0].as<protocol::transaction_id_type>();
  return _account_history_api->get_transaction( {id} );
}

DEFINE_API_IMPL( wallet_bridge_api_impl, list_witnesses )
{
  FC_ASSERT( _database_api, "database_api_plugin not enabled." );
  verify_args( args, 1 );
  FC_ASSERT(args.get_array()[0].is_array(), "list_witnesses needs at least one argument");
  const auto arguments = args.get_array()[0];
  verify_args( arguments, 2 );
  FC_ASSERT( arguments.get_array()[0].is_string(), "Lowerbound witness name is required as first argument" );
  FC_ASSERT( arguments.get_array()[1].is_numeric(), "Witnesses limit is required as second argument" );
  const string lowerbound = arguments.get_array()[0].get_string();
  const uint32_t limit = arguments.get_array()[1].as<uint32_t>();
  return _database_api->list_witnesses({lowerbound, limit, database_api::by_name});
}

DEFINE_API_IMPL( wallet_bridge_api_impl, get_witness )
{
  FC_ASSERT( _database_api, "database_api_plugin not enabled." );
  verify_args( args, 1 );
  FC_ASSERT( args.get_array()[0].is_string(), "Account name is required as first argument." );
  const protocol::account_name_type acc_name = args.get_array()[0].get_string();
  const auto& witnesses = _database_api->find_witnesses({{acc_name}}).witnesses;
  get_witness_return result;
  if (!witnesses.empty())
    result = witnesses.front();

  return result;
}

DEFINE_API_IMPL( wallet_bridge_api_impl, get_conversion_requests )
{
  FC_ASSERT( _database_api, "database_api_plugin not enabled." );
  verify_args( args, 1 );
  FC_ASSERT( args.get_array()[0].is_string(), "account name required as first argument" );
  const protocol::account_name_type account = args.get_array()[0].get_string();
  return _database_api->find_hbd_conversion_requests({account}).requests;
}

DEFINE_API_IMPL( wallet_bridge_api_impl, get_collateralized_conversion_requests )
{
  FC_ASSERT( _database_api, "database_api_plugin not enabled." );
  verify_args( args, 1 );
  FC_ASSERT( args.get_array()[0].is_string(), "account name required as first argument" );
  const protocol::account_name_type account = args.get_array()[0].get_string();
  return _database_api->find_collateralized_conversion_requests({account}).requests;
}
DEFINE_API_IMPL( wallet_bridge_api_impl, get_order_book )
{
  FC_ASSERT( _market_history_api, "market_history_api_plugin not enabled." );
  verify_args( args, 1 );
  FC_ASSERT( args.get_array()[0].is_numeric(), "Orders limit is required as first argument" );
  const uint32_t limit = args.get_array()[0].as<uint32_t>();
  return _market_history_api->get_order_book({limit});
}

DEFINE_API_IMPL( wallet_bridge_api_impl, get_open_orders )
{
  verify_args( args, 1 );
  FC_ASSERT( args.get_array()[0].is_string(), "account name is required as first argument" );
  const protocol::account_name_type account = args.get_array()[0].get_string();
  get_open_orders_return result;
  const auto& idx = _db.get_index< chain::limit_order_index, chain::by_account >();
  auto itr = idx.lower_bound( account );

  while( itr != idx.end() && itr->seller == account )
  {
    result.push_back( database_api::api_limit_order_object(*itr, _db) );
    ++itr;
  }

  return result;
}

DEFINE_API_IMPL( wallet_bridge_api_impl, get_owner_history )
{
  FC_ASSERT( _database_api, "database_api_plugin not enabled." );
  verify_args( args, 1 );
  FC_ASSERT( args.get_array()[0].is_string(), "Account name is required as first argument." );
  const protocol::account_name_type acc_name = args.get_array()[0].get_string();
  return _database_api->find_owner_histories({acc_name});
}

DEFINE_API_IMPL( wallet_bridge_api_impl, get_account_history )
{
  FC_ASSERT( _account_history_api, "account_history_api_plugin not enabled." );
  verify_args( args, 1 );
  FC_ASSERT(args.get_array()[0].is_array(), "get_account_history needs at least one argument");
  const auto arguments = args.get_array()[0];
  verify_args( arguments, 3 );
  FC_ASSERT( arguments.get_array()[0].is_string(), "Account name is required as first argument" );
  FC_ASSERT( arguments.get_array()[1].is_numeric(), "from (numeric type) is required as second argument" );
  FC_ASSERT( arguments.get_array()[2].is_numeric(), "limit (numeric type) is required as third argument" );
  const protocol::account_name_type account = arguments.get_array()[0].get_string();
  const uint32_t from = arguments.get_array()[1].as<uint32_t>();
  const uint32_t limit = arguments.get_array()[2].as<uint32_t>();
  return _account_history_api->get_account_history({account, from, limit});
}

DEFINE_API_IMPL( wallet_bridge_api_impl, list_proposals )
{
  FC_ASSERT( _database_api, "database_api_plugin not enabled." );
  verify_args( args, 1 );
  FC_ASSERT(args.get_array()[0].is_array(), "get_account_history needs at least one argument");
  const auto arguments = args.get_array()[0];
  verify_args( arguments, 5 );

  FC_ASSERT( arguments.get_array()[1].is_numeric(), "Limit is required as second argument" );
  FC_ASSERT( arguments.get_array()[2].is_numeric(), "Order type is required as third argument" );
  FC_ASSERT( arguments.get_array()[3].is_numeric(), "Order direction is required as first argument" );
  FC_ASSERT( arguments.get_array()[4].is_numeric(), "Status is required as second argument" );

  database_api::list_proposals_args api_lp_args;
  api_lp_args.start = arguments.get_array()[0];
  api_lp_args.limit = arguments.get_array()[1].as<uint32_t>();
  api_lp_args.order = arguments.get_array()[2].as<database_api::sort_order_type>();
  api_lp_args.order_direction = arguments.get_array()[3].as<database_api::order_direction_type>();
  api_lp_args.status = arguments.get_array()[4].as<database_api::proposal_status>();
  return _database_api->list_proposals(api_lp_args);
}

DEFINE_API_IMPL( wallet_bridge_api_impl, find_proposals )
{
  FC_ASSERT( _database_api, "database_api_plugin not enabled." );
  verify_args( args, 1 );
  FC_ASSERT( args.get_array()[0].is_array(), "Array of proposal ids is required as first argument" );
  const auto array_args = args.get_array()[0].get_array();
  vector<database_api::api_id_type> ids;
  ids.reserve(array_args.size());
  for (const auto& arg : array_args)
    ids.push_back(arg.as<database_api::api_id_type>());

  return _database_api->find_proposals({ids});
}

DEFINE_API_IMPL( wallet_bridge_api_impl, is_known_transaction )
{
  verify_args( args, 1 );
  FC_ASSERT( args.get_array()[0].is_string(), "Transaction id is required" );
  const protocol::transaction_id_type id = args.get_array()[0].as<protocol::transaction_id_type>();
  return _database_api->is_known_transaction({id}).is_known;
}

DEFINE_API_IMPL( wallet_bridge_api_impl, list_proposal_votes )
{
  FC_ASSERT( _database_api, "database_api_plugin not enabled." );
  verify_args( args, 1 );
  FC_ASSERT(args.get_array()[0].is_array(), "list_proposal_votes needs at least one argument");
  const auto arguments = args.get_array()[0];
  verify_args( arguments, 5 );
  FC_ASSERT( arguments.get_array()[1].is_numeric(), "Limit is required as second argument" );
  FC_ASSERT( arguments.get_array()[2].is_numeric(), "Order type is required as third argument" );
  FC_ASSERT( arguments.get_array()[3].is_numeric(), "Order direction is required as first argument" );
  FC_ASSERT( arguments.get_array()[4].is_numeric(), "Status is required as second argument" );

  database_api::list_proposal_votes_args api_lpv_args;
  api_lpv_args.start = arguments.get_array()[0];
  api_lpv_args.limit = arguments.get_array()[1].as<uint32_t>();
  api_lpv_args.order = arguments.get_array()[2].as<database_api::sort_order_type>();
  api_lpv_args.order_direction = arguments.get_array()[3].as<database_api::order_direction_type>();
  api_lpv_args.status = arguments.get_array()[4].as<database_api::proposal_status>();

  return _database_api->list_proposal_votes(api_lpv_args);
}

DEFINE_API_IMPL( wallet_bridge_api_impl, get_reward_fund )
{
  verify_args( args, 1 );
  FC_ASSERT( args.get_array()[0].is_string(), "Reward fund name is required as first argument" );
  const string reward_fund_name = args.get_array()[0].get_string();
  auto fund = _db.find< chain::reward_fund_object, chain::by_name >( reward_fund_name );
  FC_ASSERT( fund != nullptr, "Invalid reward fund name" );
  return database_api::api_reward_fund_object( *fund, _db );
}

/*
  Methods `broadcast_transaction_synchronous`, `broadcast_transaction` can be called from:
  a) `cli_wallet` internally  (non legacy )
  b) by regular API call      (non legacy or legacy( a transaction was created by `cli_wallet`)).
  It's necessary to process both situations correctly.
*/
protocol::signed_transaction wallet_bridge_api_impl::get_trx( const variant& args )
{
  FC_ASSERT( args.get_array()[0].is_object(), "Signed transaction is required as first argument" );
  try
  {
    return args.get_array()[0].as<protocol::signed_transaction>();
  }
  catch( fc::bad_cast_exception& e )
  {
    return ( args.get_array()[0].as<serializer_wrapper<protocol::signed_transaction>>() ).value;
  }
}

DEFINE_API_IMPL( wallet_bridge_api_impl, broadcast_transaction_synchronous )
{
  verify_args( args, 1 );
  /* this method is from condenser_api -> broadcast_transaction_synchronous. */
  auto tx = get_trx( args );
  FC_ASSERT( _network_broadcast_api, "network_broadcast_api_plugin not enabled." );
  FC_ASSERT( _p2p, "p2p_plugin not enabled." );

  auto txid = tx.id();
  boost::promise< broadcast_transaction_synchronous_return > p;
  {
    boost::lock_guard< boost::mutex > guard( _mtx );
    FC_ASSERT( _callbacks.find( txid ) == _callbacks.end(), "Transaction is a duplicate" );
    _callbacks[ txid ] = [&p]( const broadcast_transaction_synchronous_return& r )
    {
      p.set_value( r );
    };
    _callback_expirations[ tx.expiration ].push_back( txid );
  }

  try
  {
    /* It may look strange to call these without the lock and then lock again in the case of an exception,
     * but it is correct and avoids deadlock. accept_transaction is trained along with all other writes, including
     * pushing blocks. Pushing blocks do not originate from this code path and will never have this lock.
     * However, it will trigger the on_post_apply_block callback and then attempt to acquire the lock. In this case,
     * this thread will be waiting on accept_block so it can write and the block thread will be waiting on this
     * thread for the lock.
     */
    _chain.accept_transaction( tx );
    _p2p->broadcast_transaction( tx );
  }
  catch( fc::exception& e )
  {
    boost::lock_guard< boost::mutex > guard( _mtx );
    // The callback may have been cleared in the meantine, so we need to check for existence.
    auto c_itr = _callbacks.find( txid );
    if( c_itr != _callbacks.end() ) _callbacks.erase( c_itr );

    // We do not need to clean up _callback_expirations because on_post_apply_block handles this case.

    throw e;
  }
  catch( ... )
  {
    boost::lock_guard< boost::mutex > guard( _mtx );
    // The callback may have been cleared in the meantine, so we need to check for existence.
    auto c_itr = _callbacks.find( txid );
    if( c_itr != _callbacks.end() ) _callbacks.erase( c_itr );

    throw fc::unhandled_exception(
      FC_LOG_MESSAGE( warn, "Unknown error occured when pushing transaction" ),
      std::current_exception() );
  }

  return p.get_future().get();
}

DEFINE_API_IMPL( wallet_bridge_api_impl, broadcast_transaction )
{
  verify_args( args, 1 );
  FC_ASSERT( _network_broadcast_api, "network_broadcast_api_plugin not enabled." );
  return _network_broadcast_api->broadcast_transaction( { get_trx( args ) } );
}

DEFINE_API_IMPL( wallet_bridge_api_impl, find_recurrent_transfers )
{
  verify_args( args, 1 );
  FC_ASSERT( args.get_array()[0].is_string(), "Account name is required as first argument" );
  const string acc_name = args.get_array()[0].get_string();
  return _database_api->find_recurrent_transfers( { acc_name } ).recurrent_transfers;
}

DEFINE_API_IMPL( wallet_bridge_api_impl, find_rc_accounts )
{
  verify_args( args, 1 );
  FC_ASSERT( args.get_array()[0].is_array(), "Array of account names is required as first argument" );
  const auto array_args = args.get_array()[0].get_array();

  std::vector< protocol::account_name_type >  accounts;
  accounts.reserve(array_args.size());
  for (const auto& arg : array_args)
    accounts.push_back(arg.as<protocol::account_name_type>());

  return _rc_api->find_rc_accounts({ accounts }).rc_accounts;
}

DEFINE_API_IMPL( wallet_bridge_api_impl, list_rc_accounts )
{
  verify_args( args, 1 );
  FC_ASSERT( _rc_api, "rc_api_plugin not enabled." );
  FC_ASSERT(args.get_array()[0].is_array(), "list_rc_accounts needs at least three arguments");
  const auto arguments = args.get_array()[0];
  verify_args( arguments, 2 );
  FC_ASSERT( arguments.get_array()[0].is_string(), "Account name is required as first argument" );
  FC_ASSERT( arguments.get_array()[1].is_numeric(), "Limit is required as second argument" );

  rc::list_rc_accounts_args api_lra_args;
  api_lra_args.start = arguments.get_array()[0];
  api_lra_args.limit = arguments.get_array()[1].as<uint32_t>();

  return _rc_api->list_rc_accounts(api_lra_args).rc_accounts;
}

DEFINE_API_IMPL( wallet_bridge_api_impl, list_rc_direct_delegations )
{
  verify_args( args, 1 );
  FC_ASSERT( _rc_api, "rc_api_plugin not enabled." );
  FC_ASSERT(args.get_array()[0].is_array(), "list_rc_direct_delegations needs at least three arguments");
  const auto arguments = args.get_array()[0];
  verify_args( arguments, 2 );

  FC_ASSERT( arguments.get_array()[1].is_numeric(), "Limit is required as second argument" );

  rc::list_rc_direct_delegations_args api_lrdd_args;
  api_lrdd_args.start = arguments.get_array()[0];
  api_lrdd_args.limit = arguments.get_array()[1].as<uint32_t>();

  return _rc_api->list_rc_direct_delegations(api_lrdd_args).rc_direct_delegations;
}

DEFINE_LOCKLESS_APIS(
  wallet_bridge_api, 
  (get_version)
  (get_ops_in_block)
  (get_transaction)
  (get_account_history)
  (broadcast_transaction_synchronous)
  (broadcast_transaction)
)

DEFINE_READ_APIS(
  wallet_bridge_api,
  (get_block)
  (get_chain_properties)
  (get_witness_schedule)
  (get_current_median_history_price)
  (get_hardfork_version)
  (get_feed_history)
  (get_active_witnesses)
  (get_withdraw_routes)
  (list_my_accounts)
  (list_accounts)
  (get_dynamic_global_properties)
  (get_account)
  (get_accounts)
  (list_witnesses)
  (get_witness)
  (get_conversion_requests)
  (get_collateralized_conversion_requests)
  (get_order_book)
  (get_open_orders)
  (get_owner_history)
  (list_proposals)
  (find_proposals)
  (is_known_transaction)
  (list_proposal_votes)
  (get_reward_fund)
  (find_recurrent_transfers)
  (find_rc_accounts)
  (list_rc_accounts)
  (list_rc_direct_delegations)
)

} } } // hive::plugins::wallet_bridge_api
