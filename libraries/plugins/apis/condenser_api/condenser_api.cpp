#include <hive/plugins/condenser_api/condenser_api.hpp>
#include <hive/plugins/condenser_api/condenser_api_plugin.hpp>

#include <hive/plugins/json_rpc/utility.hpp>

#include <hive/plugins/database_api/database_api_plugin.hpp>
#include <hive/plugins/block_api/block_api_plugin.hpp>
#include <hive/plugins/account_history_api/account_history_api_plugin.hpp>
#include <hive/plugins/account_by_key_api/account_by_key_api_plugin.hpp>
#include <hive/plugins/network_broadcast_api/network_broadcast_api_plugin.hpp>
#include <hive/plugins/reputation_api/reputation_api_plugin.hpp>
#include <hive/plugins/market_history_api/market_history_api_plugin.hpp>
#include <hive/plugins/rc_api/rc_api_plugin.hpp>

#include <hive/protocol/misc_utilities.hpp>

#include <hive/utilities/git_revision.hpp>

#include <hive/chain/util/reward.hpp>
#include <hive/chain/util/uint256.hpp>

#include <fc/git_revision.hpp>

#include <boost/range/iterator_range.hpp>
#include <boost/algorithm/string.hpp>

#include <boost/thread/future.hpp>
#include <boost/thread/lock_guard.hpp>

#define CHECK_ARG_SIZE( s ) \
  FC_ASSERT( args.size() == s, "Expected #s argument(s), was ${n}", ("n", args.size()) );

#define ASSET_TO_REAL( asset ) (double)( asset.amount.value )

namespace hive { namespace plugins { namespace condenser_api {

namespace detail
{
  typedef std::function< void( const broadcast_transaction_synchronous_return& ) > confirmation_callback;

  class condenser_api_impl
  {
    public:
      condenser_api_impl() :
        _chain( appbase::app().get_plugin< hive::plugins::chain::chain_plugin >() ),
        _db( _chain.db() )
      {
        _on_post_apply_block_conn = _db.add_post_apply_block_handler(
          [&]( const block_notification& note ){ on_post_apply_block( note.block ); },
          appbase::app().get_plugin< hive::plugins::condenser_api::condenser_api_plugin >(),
          0 );
      }

      DECLARE_API_IMPL(
        (get_version)
        (get_trending_tags)
        (get_state)
        (get_active_witnesses)
        (get_block_header)
        (get_block)
        (get_ops_in_block)
        (get_config)
        (get_dynamic_global_properties)
        (get_chain_properties)
        (get_current_median_history_price)
        (get_feed_history)
        (get_witness_schedule)
        (get_hardfork_version)
        (get_next_scheduled_hardfork)
        (get_reward_fund)
        (get_key_references)
        (get_accounts)
        (get_account_references)
        (lookup_account_names)
        (lookup_accounts)
        (get_account_count)
        (get_owner_history)
        (get_recovery_request)
        (get_escrow)
        (get_withdraw_routes)
        (get_savings_withdraw_from)
        (get_savings_withdraw_to)
        (get_vesting_delegations)
        (get_expiring_vesting_delegations)
        (get_witnesses)
        (get_conversion_requests)
        (get_collateralized_conversion_requests)
        (get_witness_by_account)
        (get_witnesses_by_vote)
        (lookup_witness_accounts)
        (get_witness_count)
        (get_open_orders)
        (get_transaction_hex)
        (get_transaction)
        (get_required_signatures)
        (get_potential_signatures)
        (verify_authority)
        (verify_account_authority)
        (get_active_votes)
        (get_account_votes)
        (get_content)
        (get_content_replies)
        (get_tags_used_by_author)
        (get_post_discussions_by_payout)
        (get_comment_discussions_by_payout)
        (get_discussions_by_trending)
        (get_discussions_by_created)
        (get_discussions_by_active)
        (get_discussions_by_cashout)
        (get_discussions_by_votes)
        (get_discussions_by_children)
        (get_discussions_by_hot)
        (get_discussions_by_feed)
        (get_discussions_by_blog)
        (get_discussions_by_comments)
        (get_discussions_by_promoted)
        (get_replies_by_last_update)
        (get_discussions_by_author_before_date)
        (get_account_history)
        (broadcast_transaction)
        (broadcast_transaction_synchronous)
        (broadcast_block)
        (get_followers)
        (get_following)
        (get_follow_count)
        (get_feed_entries)
        (get_feed)
        (get_blog_entries)
        (get_blog)
        (get_account_reputations)
        (get_reblogged_by)
        (get_blog_authors)
        (get_ticker)
        (get_volume)
        (get_order_book)
        (get_trade_history)
        (get_recent_trades)
        (get_market_history)
        (get_market_history_buckets)
        (is_known_transaction)
        (list_proposals)
        (find_proposals)
        (list_proposal_votes)
        (find_recurrent_transfers)
        (find_rc_accounts)
        (list_rc_accounts)
        (list_rc_direct_delegations)
      )

      void on_post_apply_block( const signed_block& b );

      hive::plugins::chain::chain_plugin&                              _chain;

      chain::database&                                                  _db;

      std::shared_ptr< database_api::database_api >                     _database_api;
      std::shared_ptr< block_api::block_api >                           _block_api;
      std::shared_ptr< account_history::account_history_api >           _account_history_api;
      std::shared_ptr< account_by_key::account_by_key_api >             _account_by_key_api;
      std::shared_ptr< network_broadcast_api::network_broadcast_api >   _network_broadcast_api;
      p2p::p2p_plugin*                                                  _p2p = nullptr;
      std::shared_ptr< reputation::reputation_api >                     _reputation_api;
      std::shared_ptr< market_history::market_history_api >             _market_history_api;
      std::shared_ptr< rc::rc_api >                                     _rc_api;
      map< transaction_id_type, confirmation_callback >                 _callbacks;
      map< time_point_sec, vector< transaction_id_type > >              _callback_expirations;
      boost::signals2::connection                                       _on_post_apply_block_conn;

      boost::mutex                                                      _mtx;
  };

  DEFINE_API_IMPL( condenser_api_impl, get_version )
  {
    CHECK_ARG_SIZE( 0 )
    return _database_api->get_version( {} );
  }

  DEFINE_API_IMPL( condenser_api_impl, get_trending_tags )
  {
    FC_ASSERT( false, "Supported by hivemind" );
  }

  DEFINE_API_IMPL( condenser_api_impl, get_state )
  {
    FC_ASSERT( false, "Supported by hivemind" );
  }

  DEFINE_API_IMPL( condenser_api_impl, get_active_witnesses )
  {
    CHECK_ARG_SIZE( 0 )
    return _database_api->get_active_witnesses( {} ).witnesses;
  }

  DEFINE_API_IMPL( condenser_api_impl, get_block_header )
  {
    CHECK_ARG_SIZE( 1 )
    FC_ASSERT( _block_api, "block_api_plugin not enabled." );
    return _block_api->get_block_header( { args[0].as< uint32_t >() } ).header;
  }

  DEFINE_API_IMPL( condenser_api_impl, get_block )
  {
    CHECK_ARG_SIZE( 1 )
    FC_ASSERT( _block_api, "block_api_plugin not enabled." );
    get_block_return result;
    auto b = _block_api->get_block( { args[0].as< uint32_t >() } ).block;

    if( b )
    {
      result = legacy_signed_block( *b );
      uint32_t n = uint32_t( b->transactions.size() );
      uint32_t block_num = block_header::num_from_id( b->block_id );
      for( uint32_t i=0; i<n; i++ )
      {
        result->transactions[i].transaction_id = b->transactions[i].id();
        result->transactions[i].block_num = block_num;
        result->transactions[i].transaction_num = i;
      }
    }

    return result;
  }

  DEFINE_API_IMPL( condenser_api_impl, get_ops_in_block )
  {
    FC_ASSERT( args.size() == 1 || args.size() == 2, "Expected 1-2 arguments, was ${n}", ("n", args.size()) );
    FC_ASSERT( _account_history_api, "account_history_api_plugin not enabled." );

    auto ops = _account_history_api->get_ops_in_block( { args[0].as< uint32_t >(), args[1].as< bool >() } ).ops;
    get_ops_in_block_return result;

    for( auto& op_obj : ops )
    {
      result.push_back( hive::protocol::serializer_wrapper<api_operation_object>( { api_operation_object( op_obj, op_obj.op ) } ) );
      result.back().value.op_in_trx = op_obj.op_in_trx;
    }

    return result;
  }

  DEFINE_API_IMPL( condenser_api_impl, get_config )
  {
    CHECK_ARG_SIZE( 0 )
    return _database_api->get_config( {} );
  }

  DEFINE_API_IMPL( condenser_api_impl, get_dynamic_global_properties )
  {
    CHECK_ARG_SIZE( 0 )
    get_dynamic_global_properties_return gpo = _database_api->get_dynamic_global_properties( {} );

    return gpo;
  }

  DEFINE_API_IMPL( condenser_api_impl, get_chain_properties )
  {
    CHECK_ARG_SIZE( 0 )
    return api_chain_properties( _database_api->get_witness_schedule( {} ).median_props );
  }

  DEFINE_API_IMPL( condenser_api_impl, get_current_median_history_price )
  {
    CHECK_ARG_SIZE( 0 )
    return _database_api->get_current_price_feed( {} );
  }

  DEFINE_API_IMPL( condenser_api_impl, get_feed_history )
  {
    CHECK_ARG_SIZE( 0 )
    return _database_api->get_feed_history( {} );
  }

  DEFINE_API_IMPL( condenser_api_impl, get_witness_schedule )
  {
    CHECK_ARG_SIZE( 0 )
    return _database_api->get_witness_schedule( {} );
  }

  DEFINE_API_IMPL( condenser_api_impl, get_hardfork_version )
  {
    CHECK_ARG_SIZE( 0 )
    return _database_api->get_hardfork_properties( {} ).current_hardfork_version;
  }

  DEFINE_API_IMPL( condenser_api_impl, get_next_scheduled_hardfork )
  {
    CHECK_ARG_SIZE( 0 )
    scheduled_hardfork shf;
    const auto& hpo = _db.get( hardfork_property_id_type() );
    shf.hf_version = hpo.next_hardfork;
    shf.live_time = hpo.next_hardfork_time;
    return shf;
  }

  DEFINE_API_IMPL( condenser_api_impl, get_reward_fund )
  {
    CHECK_ARG_SIZE( 1 )
    string name = args[0].as< string >();

    auto fund = _db.find< reward_fund_object, by_name >( name );
    FC_ASSERT( fund != nullptr, "Invalid reward fund name" );

    return api_reward_fund_object( database_api::api_reward_fund_object( *fund, _db ) );
  }

  DEFINE_API_IMPL( condenser_api_impl, get_key_references )
  {
    CHECK_ARG_SIZE( 1 )
    FC_ASSERT( _account_by_key_api, "account_by_key_api_plugin not enabled." );

    return _account_by_key_api->get_key_references( { args[0].as< vector< public_key_type > >() } ).accounts;
  }

  DEFINE_API_IMPL( condenser_api_impl, get_accounts )
  {
    FC_ASSERT( args.size() == 1 || args.size() == 2, "Expected 1-2 arguments, was ${n}", ("n", args.size()) );
    vector< account_name_type > names = args[0].as< vector< account_name_type > >();

    bool delayed_votes_active = true;
    if( args.size() == 2 )
      delayed_votes_active = args[1].as< bool >();

    const auto& idx  = _db.get_index< account_index >().indices().get< by_name >();
    const auto& vidx = _db.get_index< witness_vote_index >().indices().get< by_account_witness >();
    vector< extended_account > results;
    results.reserve(names.size());

    for( const auto& name: names )
    {
      auto itr = idx.find( name );
      if ( itr != idx.end() )
      {
        results.emplace_back( extended_account( database_api::api_account_object( *itr, _db, delayed_votes_active ) ) );

        if(_reputation_api)
        {
          results.back().reputation = _reputation_api->get_account_reputations({ itr->name, 1 }).reputations[0].reputation;
        }

        auto vitr = vidx.lower_bound( boost::make_tuple( itr->name, account_name_type() ) );
        while( vitr != vidx.end() && vitr->account == itr->name ) {
          results.back().witness_votes.insert( _db.get< witness_object, by_name >( vitr->witness ).owner );
          ++vitr;
        }
      }
    }

    return results;
  }

  DEFINE_API_IMPL( condenser_api_impl, get_account_references )
  {
    FC_ASSERT( false, "condenser_api::get_account_references --- Needs to be refactored for Hive." );
  }

  DEFINE_API_IMPL( condenser_api_impl, lookup_account_names )
  {
    FC_ASSERT( args.size() == 1 || args.size() == 2, "Expected 1-2 arguments, was ${n}", ("n", args.size()) );
    vector< account_name_type > account_names = args[0].as< vector< account_name_type > >();

    bool delayed_votes_active = true;
    if( args.size() == 2 )
      delayed_votes_active = args[1].as< bool >();

    vector< optional< api_account_object > > result;
    result.reserve( account_names.size() );

    for( auto& name : account_names )
    {
      auto itr = _db.find< account_object, by_name >( name );

      if( itr )
      {
        result.push_back( api_account_object( database_api::api_account_object( *itr, _db, delayed_votes_active ) ) );
      }
      else
      {
        result.push_back( optional< api_account_object >() );
      }
    }

    return result;
  }

  DEFINE_API_IMPL( condenser_api_impl, lookup_accounts )
  {
    CHECK_ARG_SIZE( 2 )
    account_name_type lower_bound_name = args[0].as< account_name_type >();
    uint32_t limit = args[1].as< uint32_t >();

    FC_ASSERT( limit <= 1000 );
    const auto& accounts_by_name = _db.get_index< account_index, by_name >();
    set<string> result;

    for( auto itr = accounts_by_name.lower_bound( lower_bound_name );
        limit-- && itr != accounts_by_name.end();
        ++itr )
    {
      result.insert( itr->name );
    }

    return result;
  }

  DEFINE_API_IMPL( condenser_api_impl, get_account_count )
  {
    CHECK_ARG_SIZE( 0 )
    return _db.get_index<account_index>().indices().size();
  }

  DEFINE_API_IMPL( condenser_api_impl, get_owner_history )
  {
    CHECK_ARG_SIZE( 1 )
    return _database_api->find_owner_histories( { args[0].as< string >() } ).owner_auths;
  }

  DEFINE_API_IMPL( condenser_api_impl, get_recovery_request )
  {
    CHECK_ARG_SIZE( 1 )
    get_recovery_request_return result;

    auto requests = _database_api->find_account_recovery_requests( { { args[0].as< account_name_type >() } } ).requests;

    if( requests.size() )
      result = requests[0];

    return result;
  }

  DEFINE_API_IMPL( condenser_api_impl, get_escrow )
  {
    CHECK_ARG_SIZE( 2 )
    get_escrow_return result;

    auto escrows = _database_api->list_escrows( { { args }, 1, database_api::by_from_id } ).escrows;

    if( escrows.size()
      && escrows[0].from == args[0].as< account_name_type >()
      && escrows[0].escrow_id == args[1].as< uint32_t >() )
    {
      result = escrows[0];
    }

    return result;
  }

  DEFINE_API_IMPL( condenser_api_impl, get_withdraw_routes )
  {
    FC_ASSERT( args.size() == 1 || args.size() == 2, "Expected 1-2 arguments, was ${n}", ("n", args.size()) );

    auto account = args[0].as< string >();
    auto destination = args.size() == 2 ? args[1].as< database_api::withdraw_route_type >() : database_api::withdraw_route_type::outgoing;

    get_withdraw_routes_return result;

    if( destination == database_api::withdraw_route_type::outgoing || destination == database_api::withdraw_route_type::all )
    {
      auto routes = _database_api->find_withdraw_vesting_routes( { account, database_api::by_withdraw_route } ).routes;
      for( auto& route : routes )
        result.emplace_back( route );
    }

    if( destination == database_api::withdraw_route_type::incoming || destination == database_api::withdraw_route_type::all )
    {
      auto routes = _database_api->find_withdraw_vesting_routes( { account, database_api::by_destination } ).routes;
      for( auto& route : routes )
        result.emplace_back( route );
    }

    return result;
  }

  DEFINE_API_IMPL( condenser_api_impl, get_savings_withdraw_from )
  {
    CHECK_ARG_SIZE( 1 )

    auto withdrawals = _database_api->find_savings_withdrawals(
      {
        args[0].as< string >()
      }).withdrawals;

    get_savings_withdraw_from_return result;

    for( auto& w : withdrawals )
    {
      result.push_back( api_savings_withdraw_object( w ) );
    }

    return result;
  }

  DEFINE_API_IMPL( condenser_api_impl, get_savings_withdraw_to )
  {
    CHECK_ARG_SIZE( 1 )
    account_name_type account = args[0].as< account_name_type >();

    get_savings_withdraw_to_return result;

    const auto& to_complete_idx = _db.get_index< savings_withdraw_index, by_to_complete >();
    auto itr = to_complete_idx.lower_bound( account );
    while( itr != to_complete_idx.end() && itr->to == account )
    {
      result.emplace_back( database_api::api_savings_withdraw_object( *itr, _db ) );
      ++itr;
    }

    return result;
  }

  DEFINE_API_IMPL( condenser_api_impl, get_vesting_delegations )
  {
    FC_ASSERT( args.size() == 2 || args.size() == 3, "Expected 2-3 arguments, was ${n}", ("n", args.size()) );

    database_api::list_vesting_delegations_args a;
    account_name_type account = args[0].as< account_name_type >();
    a.start = fc::variant( (vector< variant >){ args[0], args[1] } );
    a.limit = args.size() == 3 ? args[2].as< uint32_t >() : 100;
    a.order = database_api::by_delegation;

    auto delegations = _database_api->list_vesting_delegations( a ).delegations;
    get_vesting_delegations_return result;

    for( auto itr = delegations.begin(); itr != delegations.end() && itr->delegator == account; ++itr )
    {
      result.push_back( api_vesting_delegation_object( *itr ) );
    }

    return result;
  }

  DEFINE_API_IMPL( condenser_api_impl, get_expiring_vesting_delegations )
  {
    FC_ASSERT( args.size() == 2 || args.size() == 3, "Expected 2-3 arguments, was ${n}", ("n", args.size()) );

    database_api::list_vesting_delegation_expirations_args a;
    account_name_type account = args[0].as< account_name_type >();
    a.start = fc::variant( (vector< variant >){ args[0], args[1], fc::variant( vesting_delegation_expiration_id_type() ) } );
    a.limit = args.size() == 3 ? args[2].as< uint32_t >() : 100;
    a.order = database_api::by_account_expiration;

    auto delegations = _database_api->list_vesting_delegation_expirations( a ).delegations;
    get_expiring_vesting_delegations_return result;

    for( auto itr = delegations.begin(); itr != delegations.end() && itr->delegator == account; ++itr )
    {
      result.push_back( api_vesting_delegation_expiration_object( *itr ) );
    }

    return result;
  }

  DEFINE_API_IMPL( condenser_api_impl, get_witnesses )
  {
    CHECK_ARG_SIZE( 1 )
    vector< witness_id_type > witness_ids = args[0].as< vector< witness_id_type > >();

    get_witnesses_return result;
    result.reserve( witness_ids.size() );

    std::transform(
      witness_ids.begin(),
      witness_ids.end(),
      std::back_inserter(result),
      [this](witness_id_type id) -> optional< api_witness_object >
      {
        if( auto o = _db.find(id) )
          return api_witness_object( database_api::api_witness_object ( *o, _db ) );
        return {};
      });

    return result;
  }

  DEFINE_API_IMPL( condenser_api_impl, get_conversion_requests )
  {
    CHECK_ARG_SIZE( 1 )
    auto requests = _database_api->find_hbd_conversion_requests(
      {
        args[0].as< account_name_type >()
      }).requests;

    get_conversion_requests_return result;

    for( auto& r : requests )
    {
      result.push_back( api_convert_request_object( r ) );
    }

    return result;
  }

  DEFINE_API_IMPL( condenser_api_impl, get_collateralized_conversion_requests )
  {
    CHECK_ARG_SIZE( 1 )
    auto requests = _database_api->find_collateralized_conversion_requests(
      {
        args[0].as< account_name_type >()
      }).requests;

    get_collateralized_conversion_requests_return result;

    for( auto& r : requests )
    {
      result.push_back( api_collateralized_convert_request_object( r ) );
    }

    return result;
  }

  DEFINE_API_IMPL( condenser_api_impl, get_witness_by_account )
  {
    CHECK_ARG_SIZE( 1 )
    auto witnesses = _database_api->find_witnesses(
      {
        { args[0].as< account_name_type >() }
      }).witnesses;

    get_witness_by_account_return result;

    if( witnesses.size() )
      result = api_witness_object( witnesses[0] );

    return result;
  }

  DEFINE_API_IMPL( condenser_api_impl, get_witnesses_by_vote )
  {
    CHECK_ARG_SIZE( 2 )
    account_name_type start_name = args[0].as< account_name_type >();
    vector< fc::variant > start_key;

    if( start_name == account_name_type() )
    {
      start_key.push_back( fc::variant( std::numeric_limits< int64_t >::max() ) );
      start_key.push_back( fc::variant( account_name_type() ) );
    }
    else
    {
      auto start = _database_api->list_witnesses( { args[0], 1, database_api::by_name } );

      if( start.witnesses.size() == 0 )
        return get_witnesses_by_vote_return();

      start_key.push_back( fc::variant( start.witnesses[0].votes ) );
      start_key.push_back( fc::variant( start.witnesses[0].owner ) );
    }

    auto limit = args[1].as< uint32_t >();
    auto witnesses = _database_api->list_witnesses( { fc::variant( start_key ), limit, database_api::by_vote_name } ).witnesses;

    get_witnesses_by_vote_return result;

    for( auto& w : witnesses )
    {
      result.push_back( api_witness_object( w ) );
    }

    return result;
  }

  DEFINE_API_IMPL( condenser_api_impl, lookup_witness_accounts )
  {
    CHECK_ARG_SIZE( 2 )
    auto limit = args[1].as< uint32_t >();

    lookup_witness_accounts_return result;

    auto witnesses = _database_api->list_witnesses( { args[0], limit, database_api::by_name } ).witnesses;

    for( auto& w : witnesses )
    {
      result.push_back( w.owner );
    }

    return result;
  }
  DEFINE_API_IMPL( condenser_api_impl, get_witness_count )
  {
    CHECK_ARG_SIZE( 0 )
    return _db.get_index< witness_index >().indices().size();
  }

  DEFINE_API_IMPL( condenser_api_impl, get_open_orders )
  {
    CHECK_ARG_SIZE( 1 )
    account_name_type owner = args[0].as< account_name_type >();

    vector< api_limit_order_object > result;
    const auto& idx = _db.get_index< limit_order_index, by_account >();
    auto itr = idx.lower_bound( owner );

    while( itr != idx.end() && itr->seller == owner )
    {
      result.push_back( *itr );

      if( itr->sell_price.base.symbol == HIVE_SYMBOL )
        result.back().real_price = ASSET_TO_REAL( itr->sell_price.quote ) / ASSET_TO_REAL( itr->sell_price.base );
      else
        result.back().real_price =  ASSET_TO_REAL( itr->sell_price.base ) / ASSET_TO_REAL( itr->sell_price.quote );
      ++itr;
    }

    return result;
  }

  DEFINE_API_IMPL( condenser_api_impl, get_transaction_hex )
  {
    CHECK_ARG_SIZE( 1 )
    return _database_api->get_transaction_hex(
    {
      signed_transaction( args[0].as< legacy_signed_transaction >() )
    }).hex;
  }

  DEFINE_API_IMPL( condenser_api_impl, get_transaction )
  {
    CHECK_ARG_SIZE( 1 )
    FC_ASSERT( _account_history_api, "account_history_api_plugin not enabled." );

    return hive::protocol::serializer_wrapper<signed_transaction>( { _account_history_api->get_transaction( { args[0].as< transaction_id_type >() } ) } );
  }

  DEFINE_API_IMPL( condenser_api_impl, get_required_signatures )
  {
    CHECK_ARG_SIZE( 2 )
    return _database_api->get_required_signatures( {
      signed_transaction( args[0].as< legacy_signed_transaction >() ),
      args[1].as< flat_set< public_key_type > >() } ).keys;
  }

  DEFINE_API_IMPL( condenser_api_impl, get_potential_signatures )
  {
    CHECK_ARG_SIZE( 1 )
    return _database_api->get_potential_signatures( { signed_transaction( args[0].as< legacy_signed_transaction >() ) } ).keys;
  }

  DEFINE_API_IMPL( condenser_api_impl, verify_authority )
  {
    CHECK_ARG_SIZE( 1 )
    return _database_api->verify_authority( { signed_transaction( args[0].as< legacy_signed_transaction >() ) } ).valid;
  }

  DEFINE_API_IMPL( condenser_api_impl, verify_account_authority )
  {
    CHECK_ARG_SIZE( 2 )
    return _database_api->verify_account_authority( { args[0].as< account_name_type >(), args[1].as< flat_set< public_key_type > >() } ).valid;
  }

  DEFINE_API_IMPL( condenser_api_impl, get_active_votes )
  {
    CHECK_ARG_SIZE( 2 )

    vector< vote_state > votes;
    const auto& comment = _db.get_comment( args[0].as< account_name_type >(), args[1].as< string >() );
    const auto& idx = _db.get_index< chain::comment_vote_index, chain::by_comment_voter >();
    chain::comment_id_type cid( comment.get_id() );
    auto itr = idx.lower_bound( cid );

    while( itr != idx.end() && itr->get_comment() == cid )
    {
      const auto& vo = _db.get( itr->get_voter() );
      vote_state vstate;
      vstate.voter = vo.name;
      vstate.weight = itr->get_weight();
      vstate.rshares = itr->get_rshares();
      vstate.percent = itr->get_vote_percent();
      vstate.time = itr->get_last_update();

      votes.push_back( vstate );
      ++itr;
    }

    return votes;
  }

  DEFINE_API_IMPL( condenser_api_impl, get_account_votes )
  {
    FC_ASSERT( false, "Supported by hivemind" );
  }

  DEFINE_API_IMPL( condenser_api_impl, get_content )
  {
    FC_ASSERT( false, "Supported by hivemind" );
  }

  DEFINE_API_IMPL( condenser_api_impl, get_content_replies )
  {
    FC_ASSERT( false, "Supported by hivemind" );
  }

  DEFINE_API_IMPL( condenser_api_impl, get_tags_used_by_author )
  {
    FC_ASSERT( false, "Supported by hivemind" );
  }

  DEFINE_API_IMPL( condenser_api_impl, get_post_discussions_by_payout )
  {
    FC_ASSERT( false, "Supported by hivemind" );
  }

  DEFINE_API_IMPL( condenser_api_impl, get_comment_discussions_by_payout )
  {
    FC_ASSERT( false, "Supported by hivemind" );
  }

  DEFINE_API_IMPL( condenser_api_impl, get_discussions_by_trending )
  {
    FC_ASSERT( false, "Supported by hivemind" );
  }

  DEFINE_API_IMPL( condenser_api_impl, get_discussions_by_created )
  {
    FC_ASSERT( false, "Supported by hivemind" );
  }

  DEFINE_API_IMPL( condenser_api_impl, get_discussions_by_active )
  {
    FC_ASSERT( false, "Supported by hivemind" );
  }

  DEFINE_API_IMPL( condenser_api_impl, get_discussions_by_cashout )
  {
    FC_ASSERT( false, "Supported by hivemind" );
  }

  DEFINE_API_IMPL( condenser_api_impl, get_discussions_by_votes )
  {
    FC_ASSERT( false, "Supported by hivemind" );
  }

  DEFINE_API_IMPL( condenser_api_impl, get_discussions_by_children )
  {
    FC_ASSERT( false, "Supported by hivemind" );
  }

  DEFINE_API_IMPL( condenser_api_impl, get_discussions_by_hot )
  {
    FC_ASSERT( false, "Supported by hivemind" );
  }

  DEFINE_API_IMPL( condenser_api_impl, get_discussions_by_feed )
  {
    FC_ASSERT( false, "Supported by hivemind" );
  }

  DEFINE_API_IMPL( condenser_api_impl, get_discussions_by_blog )
  {
    FC_ASSERT( false, "Supported by hivemind" );
  }

  DEFINE_API_IMPL( condenser_api_impl, get_discussions_by_comments )
  {
    FC_ASSERT( false, "Supported by hivemind" );
  }

  DEFINE_API_IMPL( condenser_api_impl, get_discussions_by_promoted )
  {
    FC_ASSERT( false, "Supported by hivemind" );
  }

  DEFINE_API_IMPL( condenser_api_impl, get_replies_by_last_update )
  {
    FC_ASSERT( false, "Supported by hivemind" );
  }

  DEFINE_API_IMPL( condenser_api_impl, get_discussions_by_author_before_date )
  {
    FC_ASSERT( false, "Supported by hivemind" );
  }

  DEFINE_API_IMPL( condenser_api_impl, get_account_history )
  {
    FC_ASSERT( args.size() == 3 || args.size() == 4 || args.size() == 5, "Expected 3, 4, or 5 argument(s), was ${n}", ("n", args.size()) );
    FC_ASSERT( _account_history_api, "account_history_api_plugin not enabled." );

    fc::optional<bool> include_reversible; /// TODO probably this shall be also included in args above
    fc::optional<uint64_t> operation_filter_low;
    if(args.size() >= 4)
      operation_filter_low = args[3].as<uint64_t>();
    fc::optional<uint64_t> operation_filter_high;
    if(args.size() >= 5)
      operation_filter_high = args[4].as<uint64_t>();

    auto history = _account_history_api->get_account_history({ args[0].as< account_name_type >(), args[1].as< uint64_t >(), args[2].as< uint32_t >(),
      include_reversible, operation_filter_low, operation_filter_high } ).history;
    get_account_history_return result;

    for( auto& entry : history )
    {
      api_operation_object obj( entry.second, entry.second.op );
      obj.op_in_trx = entry.second.op_in_trx;
      result.emplace( entry.first, hive::protocol::serializer_wrapper<api_operation_object>{ obj } );
    }

    return result;
  }

  DEFINE_API_IMPL( condenser_api_impl, broadcast_transaction )
  {
    CHECK_ARG_SIZE( 1 )
    FC_ASSERT( _network_broadcast_api, "network_broadcast_api_plugin not enabled." );
    return _network_broadcast_api->broadcast_transaction( { signed_transaction( args[0].as< legacy_signed_transaction >() ) } );
  }

  DEFINE_API_IMPL( condenser_api_impl, broadcast_transaction_synchronous )
  {
    CHECK_ARG_SIZE( 1 )
    FC_ASSERT( _network_broadcast_api, "network_broadcast_api_plugin not enabled." );
    FC_ASSERT( _p2p != nullptr, "p2p_plugin not enabled." );

    fc::time_point api_start_time = fc::time_point::now();
    signed_transaction trx = args[0].as< legacy_signed_transaction >();
    auto txid = trx.id();
    boost::promise< broadcast_transaction_synchronous_return > p;

    {
      boost::lock_guard< boost::mutex > guard( _mtx );
      FC_ASSERT( _callbacks.find( txid ) == _callbacks.end(), "Transaction is a duplicate" );
      _callbacks[ txid ] = [&p]( const broadcast_transaction_synchronous_return& r )
      {
        p.set_value( r );
      };
      _callback_expirations[ trx.expiration ].push_back( txid );
    }

    LOG_DELAY(api_start_time, fc::seconds(1), "Excessive delay to setup callback");
    fc::time_point callback_setup_time = fc::time_point::now();

    try
    {
      /* It may look strange to call these without the lock and then lock again in the case of an exception,
        * but it is correct and avoids deadlock. accept_transaction is trained along with all other writes, including
        * pushing blocks. Pushing blocks do not originate from this code path and will never have this lock.
        * However, it will trigger the on_post_apply_block callback and then attempt to acquire the lock. In this case,
        * this thread will be waiting on accept_block so it can write and the block thread will be waiting on this
        * thread for the lock.
        */
      _chain.accept_transaction( trx );
      _p2p->broadcast_transaction( trx );
    }
    catch( fc::exception& e )
    {
      LOG_DELAY_EX(callback_setup_time, fc::seconds(1), "Exccesive delay to validate & broadcast trx ${e}", (e) );

      boost::lock_guard< boost::mutex > guard( _mtx );
      // The callback may have been cleared in the meantime, so we need to check for existence.
      auto c_itr = _callbacks.find( txid );
      if( c_itr != _callbacks.end() ) _callbacks.erase( c_itr );
      // We do not need to clean up _callback_expirations because on_post_apply_block handles this case.
      throw e;
    }
    catch( ... )
    {
      LOG_DELAY(callback_setup_time, fc::seconds(1), "Excessive delay to validate & broadcast trx");

      boost::lock_guard< boost::mutex > guard( _mtx );
      // The callback may have been cleared in the meantine, so we need to check for existence.
      auto c_itr = _callbacks.find( txid );
      if( c_itr != _callbacks.end() ) _callbacks.erase( c_itr );
      throw fc::unhandled_exception(
        FC_LOG_MESSAGE( warn, "Unknown error occured when pushing transaction" ),
        std::current_exception() );
    }

    LOG_DELAY(callback_setup_time, fc::seconds(1), "Excessive delay to validate & broadcast trx");
    return p.get_future().get();
  }

  DEFINE_API_IMPL( condenser_api_impl, broadcast_block )
  {
    CHECK_ARG_SIZE( 1 )
    FC_ASSERT( _network_broadcast_api, "network_broadcast_api_plugin not enabled." );

    return _network_broadcast_api->broadcast_block( { signed_block( args[0].as< legacy_signed_block >() ) } );
  }

  DEFINE_API_IMPL( condenser_api_impl, get_followers )
  {
    FC_ASSERT( false, "Supported by hivemind" );
  }

  DEFINE_API_IMPL( condenser_api_impl, get_following )
  {
    FC_ASSERT( false, "Supported by hivemind" );
  }

  DEFINE_API_IMPL( condenser_api_impl, get_follow_count )
  {
    FC_ASSERT( false, "Supported by hivemind" );
  }

  DEFINE_API_IMPL( condenser_api_impl, get_feed_entries )
  {
    FC_ASSERT( false, "Supported by hivemind" );
  }

  DEFINE_API_IMPL( condenser_api_impl, get_feed )
  {
    FC_ASSERT( false, "Supported by hivemind" );
  }

  DEFINE_API_IMPL( condenser_api_impl, get_blog_entries )
  {
    FC_ASSERT( false, "Supported by hivemind" );
  }

  DEFINE_API_IMPL( condenser_api_impl, get_blog )
  {
    FC_ASSERT( false, "Supported by hivemind" );
  }

  DEFINE_API_IMPL( condenser_api_impl, get_account_reputations )
  {
    FC_ASSERT( args.size() == 1 || args.size() == 2, "Expected 1-2 arguments, was ${n}", ("n", args.size()) );
    FC_ASSERT( _reputation_api, "reputation_api_plugin not enabled." );

    return _reputation_api->get_account_reputations( { args[0].as< account_name_type >(), args.size() == 2 ? args[1].as< uint32_t >() : 1000 } ).reputations;
  }

  DEFINE_API_IMPL( condenser_api_impl, get_reblogged_by )
  {
    FC_ASSERT( false, "Supported by hivemind" );
  }

  DEFINE_API_IMPL( condenser_api_impl, get_blog_authors )
  {
    FC_ASSERT( false, "Supported by hivemind" );
  }

  DEFINE_API_IMPL( condenser_api_impl, get_ticker )
  {
    CHECK_ARG_SIZE( 0 )
    FC_ASSERT( _market_history_api, "market_history_api_plugin not enabled." );

    return get_ticker_return( _market_history_api->get_ticker( {} ) );
  }

  DEFINE_API_IMPL( condenser_api_impl, get_volume )
  {
    CHECK_ARG_SIZE( 0 )
    FC_ASSERT( _market_history_api, "market_history_api_plugin not enabled." );

    return get_volume_return( _market_history_api->get_volume( {} ) );
  }

  DEFINE_API_IMPL( condenser_api_impl, get_order_book )
  {
    FC_ASSERT( args.size() == 0 || args.size() == 1, "Expected 0-1 arguments, was ${n}", ("n", args.size()) );
    FC_ASSERT( _market_history_api, "market_history_api_plugin not enabled." );

    return get_order_book_return( _market_history_api->get_order_book( { args.size() == 1 ? args[0].as< uint32_t >() : 500 } ) );
  }

  DEFINE_API_IMPL( condenser_api_impl, get_trade_history )
  {
    FC_ASSERT( args.size() == 2 || args.size() == 3, "Expected 2-3 arguments, was ${n}", ("n", args.size()) );
    FC_ASSERT( _market_history_api, "market_history_api_plugin not enabled." );

    const auto& trades = _market_history_api->get_trade_history( { args[0].as< time_point_sec >(), args[1].as< time_point_sec >(), args.size() == 3 ? args[2].as< uint32_t >() : 1000 } ).trades;
    get_trade_history_return result;

    for( const auto& t : trades ) result.push_back( market_trade( t ) );

    return result;
  }

  DEFINE_API_IMPL( condenser_api_impl, get_recent_trades )
  {
    FC_ASSERT( args.size() == 0 || args.size() == 1, "Expected 0-1 arguments, was ${n}", ("n", args.size()) );
    FC_ASSERT( _market_history_api, "market_history_api_plugin not enabled." );

    const auto& trades = _market_history_api->get_recent_trades( { args.size() == 1 ? args[0].as< uint32_t >() : 1000 } ).trades;
    get_trade_history_return result;

    for( const auto& t : trades ) result.push_back( market_trade( t ) );

    return result;
  }

  DEFINE_API_IMPL( condenser_api_impl, get_market_history )
  {
    CHECK_ARG_SIZE( 3 )
    FC_ASSERT( _market_history_api, "market_history_api_plugin not enabled." );

    return _market_history_api->get_market_history( { args[0].as< uint32_t >(), args[1].as< time_point_sec >(), args[2].as< time_point_sec >() } ).buckets;
  }

  DEFINE_API_IMPL( condenser_api_impl, get_market_history_buckets )
  {
    CHECK_ARG_SIZE( 0 )
    FC_ASSERT( _market_history_api, "market_history_api_plugin not enabled." );

    return _market_history_api->get_market_history_buckets( {} ).bucket_sizes;
  }

  DEFINE_API_IMPL( condenser_api_impl, is_known_transaction )
  {
    CHECK_ARG_SIZE( 1 )

    return _database_api->is_known_transaction( { args[0].as<transaction_id_type>() } ).is_known;
  }

  DEFINE_API_IMPL( condenser_api_impl, list_proposals )
  {
    FC_ASSERT( args.size() >= 3 && args.size() <= 6, "Expected 3-6 argument, was ${n}", ("n", args.size()) );

    hive::plugins::database_api::list_proposals_args list_args;
    list_args.start           = args[0];
    list_args.limit           = args[1].as< uint32_t >();
    list_args.order           = args[2].as< hive::plugins::database_api::sort_order_type >();
    list_args.order_direction = args.size() > 3 ?
      args[3].as< hive::plugins::database_api::order_direction_type >() : database_api::ascending;
    list_args.status          = args.size() > 4 ?
      args[4].as< hive::plugins::database_api::proposal_status >() : database_api::all;
    list_args.last_id         = args.size() > 5 ?
      fc::optional<uint64_t>( args[5].as< uint64_t >() ) : fc::optional<uint64_t>();

    const auto& proposals = _database_api->list_proposals( list_args ).proposals;
    list_proposals_return result;

    for( const auto& p : proposals ) result.emplace_back( api_proposal_object( p ) );

    return result;
  }

  DEFINE_API_IMPL( condenser_api_impl, find_proposals )
  {
    CHECK_ARG_SIZE( 1 )

    const auto& proposals = _database_api->find_proposals( { args[0].as< vector< hive::plugins::database_api::api_id_type > >() } ).proposals;
    find_proposals_return result;

    for( const auto& p : proposals ) result.emplace_back( api_proposal_object( p ) );

    return result;
  }

  DEFINE_API_IMPL( condenser_api_impl, list_proposal_votes )
  {
    FC_ASSERT( args.size() >= 3 && args.size() <= 5, "Expected 3-5 argument, was ${n}", ("n", args.size()) );

    hive::plugins::database_api::list_proposals_args list_args;
    list_args.start           = args[0];
    list_args.limit           = args[1].as< uint32_t >();
    list_args.order           = args[2].as< hive::plugins::database_api::sort_order_type >();
    list_args.order_direction = args.size() > 3 ?
      args[3].as< hive::plugins::database_api::order_direction_type >() : database_api::ascending;
    list_args.status          = args.size() > 4 ?
      args[4].as< hive::plugins::database_api::proposal_status >() : database_api::all;

    return _database_api->list_proposal_votes( list_args ).proposal_votes;
  }


  DEFINE_API_IMPL( condenser_api_impl, find_recurrent_transfers )
  {
    CHECK_ARG_SIZE( 1 )

    return _database_api->find_recurrent_transfers( { args[0].as< account_name_type >() } ).recurrent_transfers;
  }

  void condenser_api_impl::on_post_apply_block( const signed_block& b )
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
        itr->second( broadcast_transaction_synchronous_return( id, block_num, int32_t( trx_num ), false ) );
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
      for( const transaction_id_type& txid : exp_it->second )
      {
        auto cb_it = _callbacks.find( txid );
        // If it's empty, that means the transaction has been confirmed and has been deleted by the above check.
        if( cb_it == _callbacks.end() )
          continue;

        confirmation_callback callback = cb_it->second;
        transaction_id_type txid_byval = txid;    // can't pass in by reference as it's going to be deleted
        callback( broadcast_transaction_synchronous_return( txid_byval, block_num, -1, true ) );

        _callbacks.erase( cb_it );
      }
      _callback_expirations.erase( exp_it );
    }
  } FC_LOG_AND_RETHROW() }

  DEFINE_API_IMPL( condenser_api_impl, find_rc_accounts )
  {
    CHECK_ARG_SIZE( 1 )
    FC_ASSERT( _rc_api, "rc_api_plugin not enabled." );
    return _rc_api->find_rc_accounts( { args[0].as< vector< account_name_type > >() } ).rc_accounts;
  }

  DEFINE_API_IMPL( condenser_api_impl, list_rc_accounts )
  {
    FC_ASSERT( args.size() == 3, "Expected 3 arguments, was ${n}", ("n", args.size()) );
    FC_ASSERT( _rc_api, "rc_api_plugin not enabled." );
    rc::list_rc_accounts_args a;
    a.start = args[0].as< account_name_type >();
    a.limit = args[1].as< uint32_t >();
    return _rc_api->list_rc_accounts( a ).rc_accounts;
  }

  DEFINE_API_IMPL( condenser_api_impl, list_rc_direct_delegations )
  {
    FC_ASSERT( args.size() == 3, "Expected 3 arguments, was ${n}", ("n", args.size()) );
    FC_ASSERT( _rc_api, "rc_api_plugin not enabled." );
    rc::list_rc_direct_delegations_args a;
    a.start = args[0].as< vector< fc::variant > >();
    a.limit = args[1].as< uint32_t >();
    return _rc_api->list_rc_direct_delegations( a ).rc_direct_delegations;
  }

} // detail

uint16_t api_account_object::_compute_voting_power( const database_api::api_account_object& a )
{
  if( a.voting_manabar.last_update_time < HIVE_HARDFORK_0_20_TIME )
    return (uint16_t) a.voting_manabar.current_mana;

  auto vests = chain::util::get_effective_vesting_shares( a );
  if( vests <= 0 )
    return 0;

  //
  // Let t1 = last_vote_time, t2 = last_update_time
  // vp_t2 = HIVE_100_PERCENT * current_mana / vests
  // vp_t1 = vp_t2 - HIVE_100_PERCENT * (t2 - t1) / HIVE_VOTING_MANA_REGENERATION_SECONDS
  //

  uint32_t t1 = a.last_vote_time.sec_since_epoch();
  uint32_t t2 = a.voting_manabar.last_update_time;
  uint64_t dt = (t2 > t1) ? (t2 - t1) : 0;
  uint64_t vp_dt = HIVE_100_PERCENT * dt / HIVE_VOTING_MANA_REGENERATION_SECONDS;

  uint128_t vp_t2 = HIVE_100_PERCENT;
  vp_t2 *= a.voting_manabar.current_mana;
  vp_t2 /= vests;

  uint64_t vp_t2u = vp_t2.to_uint64();
  if( vp_t2u >= HIVE_100_PERCENT )
  {
    wlog( "Truncated vp_t2u to HIVE_100_PERCENT for account ${a}", ("a", a.name) );
    vp_t2u = HIVE_100_PERCENT;
  }
  uint16_t vp_t1 = uint16_t( vp_t2u ) - uint16_t( std::min( vp_t2u, vp_dt ) );

  return vp_t1;
}

condenser_api::condenser_api()
  : my( new detail::condenser_api_impl() )
{
  JSON_RPC_REGISTER_API( HIVE_CONDENSER_API_PLUGIN_NAME );
}

condenser_api::~condenser_api() {}

void condenser_api::api_startup()
{
  auto database = appbase::app().find_plugin< database_api::database_api_plugin >();
  if( database != nullptr )
  {
    my->_database_api = database->api;
  }

  auto block = appbase::app().find_plugin< block_api::block_api_plugin >();
  if( block != nullptr )
  {
    my->_block_api = block->api;
  }

  auto account_by_key = appbase::app().find_plugin< account_by_key::account_by_key_api_plugin >();
  if( account_by_key != nullptr )
  {
    my->_account_by_key_api = account_by_key->api;
  }

  auto account_history = appbase::app().find_plugin< account_history::account_history_api_plugin >();
  if( account_history != nullptr )
  {
    my->_account_history_api = account_history->api;
  }

  auto network_broadcast = appbase::app().find_plugin< network_broadcast_api::network_broadcast_api_plugin >();
  if( network_broadcast != nullptr )
  {
    my->_network_broadcast_api = network_broadcast->api;
  }

  auto p2p = appbase::app().find_plugin< p2p::p2p_plugin >();
  if( p2p != nullptr )
  {
    my->_p2p = p2p;
  }

  auto reputation = appbase::app().find_plugin< reputation::reputation_api_plugin >();
  if( reputation != nullptr )
  {
    my->_reputation_api = reputation->api;
  }

  auto market_history = appbase::app().find_plugin< market_history::market_history_api_plugin >();
  if( market_history != nullptr )
  {
    my->_market_history_api = market_history->api;
  }

  auto rc = appbase::app().find_plugin< rc::rc_api_plugin >();
  if( rc != nullptr )
  {
    my->_rc_api = rc->api;
  }
}

DEFINE_LOCKLESS_APIS( condenser_api,
  (get_version)
  (get_config)
  (get_account_references)
  (broadcast_transaction)
  (broadcast_transaction_synchronous)
  (broadcast_block)
  (get_market_history_buckets)
  (get_ops_in_block)
  (get_account_history)
  (get_transaction)
)

DEFINE_READ_APIS( condenser_api,
  (get_trending_tags)
  (get_state)
  (get_active_witnesses)
  (get_block_header)
  (get_block)
  (get_dynamic_global_properties)
  (get_chain_properties)
  (get_current_median_history_price)
  (get_feed_history)
  (get_witness_schedule)
  (get_hardfork_version)
  (get_next_scheduled_hardfork)
  (get_reward_fund)
  (get_key_references)
  (get_accounts)
  (lookup_account_names)
  (lookup_accounts)
  (get_account_count)
  (get_owner_history)
  (get_recovery_request)
  (get_escrow)
  (get_withdraw_routes)
  (get_savings_withdraw_from)
  (get_savings_withdraw_to)
  (get_vesting_delegations)
  (get_expiring_vesting_delegations)
  (get_witnesses)
  (get_conversion_requests)
  (get_collateralized_conversion_requests)
  (get_witness_by_account)
  (get_witnesses_by_vote)
  (lookup_witness_accounts)
  (get_witness_count)
  (get_open_orders)
  (get_transaction_hex)
  (get_required_signatures)
  (get_potential_signatures)
  (verify_authority)
  (verify_account_authority)
  (get_active_votes)
  (get_account_votes)
  (get_content)
  (get_content_replies)
  (get_tags_used_by_author)
  (get_post_discussions_by_payout)
  (get_comment_discussions_by_payout)
  (get_discussions_by_trending)
  (get_discussions_by_created)
  (get_discussions_by_active)
  (get_discussions_by_cashout)
  (get_discussions_by_votes)
  (get_discussions_by_children)
  (get_discussions_by_hot)
  (get_discussions_by_feed)
  (get_discussions_by_blog)
  (get_discussions_by_comments)
  (get_discussions_by_promoted)
  (get_replies_by_last_update)
  (get_discussions_by_author_before_date)
  (get_followers)
  (get_following)
  (get_follow_count)
  (get_feed_entries)
  (get_feed)
  (get_blog_entries)
  (get_blog)
  (get_account_reputations)
  (get_reblogged_by)
  (get_blog_authors)
  (get_ticker)
  (get_volume)
  (get_order_book)
  (get_trade_history)
  (get_recent_trades)
  (get_market_history)
  (is_known_transaction)
  (list_proposals)
  (list_proposal_votes)
  (find_proposals)
  (find_recurrent_transfers)
  (find_rc_accounts)
  (list_rc_accounts)
  (list_rc_direct_delegations)
)

} } } // hive::plugins::condenser_api
