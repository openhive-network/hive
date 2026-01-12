#include <hive/chain/hive_fwd.hpp>

#include <hive/protocol/fixed_string.hpp>

#include <hive/chain/hive_evaluator.hpp>
#include <hive/chain/database.hpp>
#include <hive/chain/database_virtual_operations.hpp>
#include <hive/chain/detail/state/decline_voting_rights_request_object_multiindex.hpp>
#include <hive/chain/global_property_object_multiindex.hpp>
#include <hive/chain/witness_objects_multiindex.hpp>
#include <hive/chain/account_object_multiindex.hpp>
#include <hive/chain/evaluator_registry.hpp>
#include <hive/chain/assets_object.hpp>
#include <hive/chain/recovery_object.hpp>
#include <hive/chain/time_object.hpp>
#include <hive/chain/manabars_rc_object.hpp>
#include <hive/chain/delayed_votes_object.hpp>

#include <hive/chain/util/owner_update_limit_mgr.hpp>

#include <fc/uint128.hpp>

namespace hive { namespace chain {

HIVE_DEFINE_EVALUATOR( account_create )
HIVE_DEFINE_EVALUATOR( account_create_with_delegation )
HIVE_DEFINE_EVALUATOR( account_update )
HIVE_DEFINE_EVALUATOR( account_update2 )
HIVE_DEFINE_EVALUATOR( claim_account )
HIVE_DEFINE_EVALUATOR( create_claimed_account )
HIVE_DEFINE_EVALUATOR( request_account_recovery )
HIVE_DEFINE_EVALUATOR( recover_account )
HIVE_DEFINE_EVALUATOR( change_recovery_account )
HIVE_DEFINE_EVALUATOR( decline_voting_rights )
HIVE_DEFINE_EVALUATOR( reset_account )
HIVE_DEFINE_EVALUATOR( set_reset_account )

void register_account_evaluators( evaluator_registry<operation>& registry )
{
  registry.register_evaluator< account_create_evaluator                 >();
  registry.register_evaluator< account_create_with_delegation_evaluator >();
  registry.register_evaluator< account_update_evaluator                 >();
  registry.register_evaluator< account_update2_evaluator                >();
  registry.register_evaluator< claim_account_evaluator                  >();
  registry.register_evaluator< create_claimed_account_evaluator         >();
  registry.register_evaluator< request_account_recovery_evaluator       >();
  registry.register_evaluator< recover_account_evaluator                >();
  registry.register_evaluator< change_recovery_account_evaluator        >();
  registry.register_evaluator< decline_voting_rights_evaluator          >();
  registry.register_evaluator< reset_account_evaluator                  >();
  registry.register_evaluator< set_reset_account_evaluator              >();
}

std::list<account_name_type> verify_authority_accounts_exist_impl(
  const database& db,
  const authority& auth,
  const account_name_type& auth_account,
  authority::classification auth_class,
  bool is_required)
{
  std::list<account_name_type> _incorrect_accounts;
  for( const std::pair< account_name_type, weight_type >& aw : auth.account_auths )
  {
    const account_object* a = db.find_account( aw.first );
    if( is_required )
    {
      FC_ASSERT( a != nullptr, "New ${ac} authority on account ${aa} references non-existing account ${aref}",
        ("aref", aw.first)("ac", auth_class)("aa", auth_account) );
    }
    else
    {
      if( a == nullptr )
      {
        _incorrect_accounts.push_back( aw.first );
        ilog( "New ${ac} authority on account ${aa} references non-existing account ${aref}",
          ("aref", aw.first)("ac", auth_class)("aa", auth_account) );
      }
    }
  }
  return _incorrect_accounts;
}

void verify_authority_accounts_exist(
  const database& db,
  const authority& auth,
  const account_name_type& auth_account,
  authority::classification auth_class)
{
  verify_authority_accounts_exist_impl( db, auth, auth_account, auth_class, true/*is_required*/ );
}

optional< authority > check_authority_accounts_exist(
  const database& db,
  const authority& auth,
  const account_name_type& auth_account,
  authority::classification auth_class)
{
  std::list<account_name_type> _incorrect_accounts = verify_authority_accounts_exist_impl( db, auth, auth_account, auth_class, false/*is_required*/ );

  if( !_incorrect_accounts.empty() )
  {
    optional< authority > _new_auth = auth;
    /*
      In whole blockchain there are only a few transactions, where an account is incorrect.

      For account `jesus2`:
        block;trx_id

        3705111;45c95c0ee2225a98622fc2cf38cdda7de0958098
        3705120;6af2fce04650be03c0fca721fa815404fed64f48
        3713940;3ddfebf47900a9347e885e6cdeeee3dd8a0904b6
        3714132;26aee948ac28e4b93ce703469ef07d29dcef60a6
        3714567;229aadb42374ff068ef8982045ae55328fcca7db
        3714588;b808b95c61a5d734b863a53e99d25170fe416772

      For account `testing001`:
      block;trx_id

      4138790;7c96d0c6f17359ff671e40155095fb72fea9a0cc

    */
    for( const auto& account: _incorrect_accounts )
    {
      _new_auth->account_auths.erase( account );
    }

    return _new_auth;
  }

  return optional< authority >();
}

const account_object& create_account( database& db, const account_name_type& name, const public_key_type& key,
  const time_point_sec& _creation_time, const time_point_sec& _block_creation_time, bool mined, const HIVE_asset& fee_for_rc_adjustment,
  const account_object* recovery_account, const VEST_asset& initial_delegation )
{
  if( db.has_hardfork( HIVE_HARDFORK_0_11 ) )
  {
    if( recovery_account && recovery_account->get_name() == HIVE_TEMP_ACCOUNT )
      recovery_account = nullptr;
  }
  else
  {
    recovery_account = &db.get_account( "steem" ); //not using find_account to make sure "steem" already exists
  }

  int64_t rc_adjustment_from_fee = 0; // accounts created prior to HF20 have all RC related data set during HF20
  if( db.has_hardfork( HIVE_HARDFORK_0_20 ) )
  {
    const auto& dgpo = db.get_dynamic_global_properties();
    rc_adjustment_from_fee = ( fee_for_rc_adjustment * dgpo.get_vesting_share_price() ).amount.value;
  }

  FC_ASSERT( db.find_account( name ) == nullptr, "Account ${name} already exists.", ( name ) );

  // Create the account_object with new minimal constructor
  const account_object& new_account = db.create< account_object >( name, key, _creation_time, _block_creation_time, mined, recovery_account );

  // Create the related objects with the same account_id
  bool mana_100_percent = !db.has_hardfork( HIVE_HARDFORK_0_20__2539 );

  db.create< assets_object >( new_account.get_id(), initial_delegation );
  db.create< recovery_object >( new_account.get_id(), recovery_account ? recovery_account->get_id() : account_id_type() );
  db.create< time_object >( new_account.get_id() );
  db.create< manabars_rc_object >( new_account.get_id(), _creation_time, mana_100_percent, rc_adjustment_from_fee );
  db.create< delayed_votes_object >( new_account.get_id() );

  return new_account;
}

void account_create_evaluator::do_apply( const account_create_operation& o )
{
  const auto& creator = _db.get_account( o.creator );

  const auto& props = _db.get_dynamic_global_properties();

  const witness_schedule_object& wso = _db.get_witness_schedule_object();

  HIVE_asset o_fee = o.get_fee();

  if( _db.has_hardfork( HIVE_HARDFORK_0_20__1771 ) )
  {
    FC_ASSERT( o_fee == wso.median_props.account_creation_fee, "Must pay the exact account creation fee. paid: ${p} fee: ${f}",
      ( "p", o_fee )( "f", wso.median_props.account_creation_fee ) );
  }
  else if( !_db.has_hardfork( HIVE_HARDFORK_0_20__1761 ) && _db.has_hardfork( HIVE_HARDFORK_0_19__987 ) )
  {
    FC_ASSERT( o_fee >= HIVE_asset( wso.median_props.account_creation_fee.amount * HIVE_CREATE_ACCOUNT_WITH_HIVE_MODIFIER ), "Insufficient Fee: ${f} required, ${p} provided.",
      ( "f", HIVE_asset( wso.median_props.account_creation_fee.amount * HIVE_CREATE_ACCOUNT_WITH_HIVE_MODIFIER ) )( "p", o_fee ) );
  }
  else if( _db.has_hardfork( HIVE_HARDFORK_0_1 ) )
  {
    FC_ASSERT( o_fee >= wso.median_props.account_creation_fee && "Can't create", "Insufficient Fee: ${f} required, ${p} provided.",
      ( "f", wso.median_props.account_creation_fee )( "p", o_fee ) );
  }

  verify_authority_accounts_exist( _db, o.owner, o.new_account_name, authority::owner );
  verify_authority_accounts_exist( _db, o.active, o.new_account_name, authority::active );
  verify_authority_accounts_exist( _db, o.posting, o.new_account_name, authority::posting );

  _db.adjust_balance( creator, -o_fee );

  const auto& new_account = create_account( _db, o.new_account_name, o.memo_key, props.time, _db.get_current_timestamp(),
    false /*mined*/, o_fee, &creator );

  VEST_asset initial_vesting_shares( 0 );
  if( _db.has_hardfork( HIVE_HARDFORK_0_20__1762 ) )
    _db.adjust_balance( _db.get< account_object, by_name >( HIVE_NULL_ACCOUNT ), o_fee );
  else if( o_fee.amount > 0 )
    initial_vesting_shares = _db.create_vesting( new_account, o_fee );

  _db.create< account_authority_object >( [&]( account_authority_object& auth )
  {
    auth.account = o.new_account_name;
    auth.owner = o.owner;
    auth.active = o.active;
    auth.posting = o.posting;
    auth.previous_owner_update = fc::time_point_sec::min();
    auth.last_owner_update = fc::time_point_sec::min();
  } );

  push_virtual_operation( _db, account_created_operation( o.new_account_name, o.creator, initial_vesting_shares, VEST_asset( 0 ) ) );
}

void account_create_with_delegation_evaluator::do_apply( const account_create_with_delegation_operation& o )
{
  FC_ASSERT( !_db.has_hardfork( HIVE_HARDFORK_0_20__1760 ), "Account creation with delegation is deprecated as of Hardfork 20" );

  const auto& creator = _db.get_account( o.creator );
  const auto& props = _db.get_dynamic_global_properties();
  const witness_schedule_object& wso = _db.get_witness_schedule_object();

  VEST_asset o_delegation = o.get_delegation();
  HIVE_asset o_fee = o.get_fee();

  FC_ASSERT( creator.get_balance() >= o_fee && "Can't create",
    "Insufficient balance to create account.",
    ( "creator.balance", creator.get_balance() )
    ( "required", o_fee ) );

  FC_ASSERT( creator.get_vesting() - creator.get_delegated_vesting() - VEST_asset( creator.get_total_vesting_withdrawal() ) >= o_delegation, "Insufficient vesting shares to delegate to new account.",
    ( "creator.vesting_shares", creator.get_vesting() )( "creator.delegated_vesting_shares", creator.get_delegated_vesting() )( "required", o_delegation ) );

  VEST_asset target_delegation = ( wso.median_props.account_creation_fee * ( HIVE_CREATE_ACCOUNT_WITH_HIVE_MODIFIER * HIVE_CREATE_ACCOUNT_DELEGATION_RATIO ) ) * props.get_vesting_share_price();

  VEST_asset current_delegation = ( o_fee * HIVE_CREATE_ACCOUNT_DELEGATION_RATIO ) * props.get_vesting_share_price() + o_delegation;

  FC_ASSERT( current_delegation >= target_delegation, "Insufficient Delegation ${f} required, ${p} provided.",
    ( "f", target_delegation )( "p", current_delegation )( "account_creation_fee", wso.median_props.account_creation_fee )( "o.fee", o_fee )( "o.delegation", o_delegation ) );

  FC_ASSERT( o_fee >= wso.median_props.account_creation_fee, "Insufficient Fee: ${f} required, ${p} provided.",
    ( "f", wso.median_props.account_creation_fee )( "p", o_fee ) );

  verify_authority_accounts_exist( _db, o.owner, o.new_account_name, authority::owner );
  verify_authority_accounts_exist( _db, o.active, o.new_account_name, authority::active );
  verify_authority_accounts_exist( _db, o.posting, o.new_account_name, authority::posting );

  _db.modify( creator, [&]( account_object& c )
  {
    c.balance -= o_fee;
    c.delegated_vesting_shares += o_delegation;
  } );

  const auto& new_account = create_account( _db, o.new_account_name, o.memo_key, props.time, _db.get_current_timestamp(),
    false /*mined*/, o_fee, &creator, o_delegation );

  VEST_asset initial_vesting_shares( 0 );
  if( _db.has_hardfork( HIVE_HARDFORK_0_20__1762 ) )
    _db.adjust_balance( _db.get< account_object, by_name >( HIVE_NULL_ACCOUNT ), o_fee );
  else if( o_fee.amount > 0 )
    initial_vesting_shares = _db.create_vesting( new_account, o_fee );

  _db.create< account_authority_object >( [&]( account_authority_object& auth )
  {
    auth.account = o.new_account_name;
    auth.owner = o.owner;
    auth.active = o.active;
    auth.posting = o.posting;
    auth.previous_owner_update = fc::time_point_sec::min();
    auth.last_owner_update = fc::time_point_sec::min();
  } );

  if( o_delegation.amount > 0 || !_db.has_hardfork( HIVE_HARDFORK_0_19__997 ) )
  {
    //ABW: for future reference:
    //the above HF19 condition cannot be eliminated, because when delegation is created here, its minimal time
    //is 30 days, while delegation created normally will have no effective limit;
    //this means that there is difference between zero delegation created here and then increased with
    //delegate_vesting_shares_operation and delegation created with delegate_vesting_shares_operation;
    //if such delegation is then reduced, delegated VESTs will return to delegator at different time, in case
    //30 days after creation happens to be later than "now - dgpo.delegation_return_period" at the time when
    //delegation is reduced; such situation actually happened
    _db.create< vesting_delegation_object >( creator, new_account, o_delegation,
      _db.head_block_time() + HIVE_CREATE_ACCOUNT_DELEGATION_TIME );
  }

  push_virtual_operation( _db, account_created_operation( o.new_account_name, o.creator, initial_vesting_shares, o_delegation ) );
}


void account_update_evaluator::do_apply( const account_update_operation& o )
{
  FC_ASSERT(o.account != HIVE_TEMP_ACCOUNT, "Cannot update temp account.");

  const auto& account = _db.get_account( o.account );
  const auto& account_auth = _db.get< account_authority_object, by_account >( o.account );

  if( _db.has_hardfork( HIVE_HARDFORK_0_20 ) ) // 3920d6b938492a873b36b3b44725154cf4123e95 example authority exceeding size (hellosteem has even bigger set to this day)
  {
    if( o.owner )
      validate_auth_size( *o.owner );
    if( o.active )
      validate_auth_size( *o.active );
    if( o.posting )
      validate_auth_size( *o.posting );
  }

  if( o.owner )
  {
// Blockchain converter uses the `account_update` operation to change the private keys of the pow-mined accounts within the same transaction
// Due to that the value of `last_owner_update` is (in a standard environment) less than `HIVE_OWNER_UPDATE_LIMIT`
// (see the blockchain converter `post_convert_transaction` function to understand the actual reason for this directive here)
// Note: There is a similar `ifndef` directive in the `do_apply( account_update2_operation )` function for the `IS_TEST_NET` identifier,
//       but not for the `HIVE_CONVERTER_BUILD`, because mining was removed in HF17 and the `account_update2` operation was first introduced
//       in HF21, so only this operation is applicable to our needs
# ifndef HIVE_CONVERTER_BUILD
    if( _db.has_hardfork( HIVE_HARDFORK_0_11 ) )
      FC_ASSERT( util::owner_update_limit_mgr::check( _db.has_hardfork( HIVE_HARDFORK_1_26_AUTH_UPDATE ), _db.head_block_time(),
                                                                        account_auth.previous_owner_update, account_auth.last_owner_update ) && "update",
                                                      "${m}", ("m", util::owner_update_limit_mgr::msg( _db.has_hardfork( HIVE_HARDFORK_1_26_AUTH_UPDATE ) )) );
# endif

    verify_authority_accounts_exist( _db, *o.owner, o.account, authority::owner );
    _db.update_owner_authority( account, *o.owner );
  }

  if( o.active )
    verify_authority_accounts_exist( _db, *o.active, o.account, authority::active );

  const optional< authority >* _auth_posting = &o.posting;
  optional< authority > _corrected_posting;
  if( o.posting )
  {
    if( _db.has_hardfork( HIVE_HARDFORK_0_15__465 ) ) // see check_authority_accounts_exist
    {
      verify_authority_accounts_exist( _db, *o.posting, o.account, authority::posting );
    }
    else
    {
      _corrected_posting = check_authority_accounts_exist( _db, *o.posting, o.account, authority::posting );
      if( _corrected_posting )
        _auth_posting = &_corrected_posting;
    }
  }

  if( o.memo_key != public_key_type() )
  {
    _db.modify( account, [&]( account_object& acc )
    {
      acc.set_memo_key( o.memo_key );
    } );
  }

  const auto& account_time = _db.get< time_object, by_account_id >( account.get_id() );
  _db.modify( account_time, [&]( time_object& t )
  {
    t.set_last_account_update( _db.head_block_time() );
  } );

  if( o.active || *_auth_posting )
  {
    _db.modify( account_auth, [&]( account_authority_object& auth)
    {
      if( o.active )       auth.active  = *o.active;
      if( *_auth_posting ) auth.posting = **_auth_posting;
    } );
  }

}

void account_update2_evaluator::do_apply( const account_update2_operation& o )
{
  FC_ASSERT( _db.has_hardfork( HIVE_HARDFORK_0_21__3274 ), "Operation 'account_update2' is not enabled until HF 21" );
  FC_ASSERT( o.account != HIVE_TEMP_ACCOUNT && "Cannot update temp account." );

  const auto& account = _db.get_account( o.account );
  const auto& account_auth = _db.get< account_authority_object, by_account >( o.account );

  if( o.owner )
  {
    FC_ASSERT( util::owner_update_limit_mgr::check( _db.has_hardfork( HIVE_HARDFORK_1_26_AUTH_UPDATE ), _db.head_block_time(),
                                                                      account_auth.previous_owner_update, account_auth.last_owner_update ) && "update2",
                                                    "${m}", ("m", util::owner_update_limit_mgr::msg( _db.has_hardfork( HIVE_HARDFORK_1_26_AUTH_UPDATE ) ) ) );

    verify_authority_accounts_exist( _db, *o.owner, o.account, authority::owner );

    _db.update_owner_authority( account, *o.owner );
  }
  if( o.active )
    verify_authority_accounts_exist( _db, *o.active, o.account, authority::active );
  if( o.posting )
    verify_authority_accounts_exist( _db, *o.posting, o.account, authority::posting );

  if( o.memo_key && *o.memo_key != public_key_type() )
  {
    _db.modify( account, [&]( account_object& acc )
    {
      acc.set_memo_key( *o.memo_key );
    } );
  }

  const auto& account_time = _db.get< time_object, by_account_id >( account.get_id() );
  _db.modify( account_time, [&]( time_object& t )
  {
    t.set_last_account_update( _db.head_block_time() );
  } );

  if( o.active || o.posting )
  {
    _db.modify( account_auth, [&]( account_authority_object& auth)
    {
      if( o.active )  auth.active  = *o.active;
      if( o.posting ) auth.posting = *o.posting;
    } );
  }

}

void claim_account_evaluator::do_apply( const claim_account_operation& o )
{
  FC_ASSERT( _db.has_hardfork( HIVE_HARDFORK_0_20__1771 ) && "claim_account_operation is not enabled until hardfork 20." );

  const auto& creator = _db.get_account( o.creator );
  const auto& wso = _db.get_witness_schedule_object();

  HIVE_asset o_fee = o.get_fee();

  FC_ASSERT( creator.get_balance() >= o_fee && "Can't claim",
    "Insufficient balance to create account.",
    ( "creator.balance", creator.get_balance() )( "required", o_fee ) );

  if( o_fee.amount == 0 )
  {
    const auto& gpo = _db.get_dynamic_global_properties();

    // since RC is nonconsensus, a rogue witness could easily pass transactions that would drain global
    // subsidy pool; to prevent that we need to limit such misbehavior to his own share of subsidies;
    // however if we always did the check, transactions that arrive at the node in time when head block
    // was signed by witness with exhausted subsidies, would all be dropped and not propagated by p2p;
    // therefore we only do the check when we can attribute transaction to concrete witness, that it,
    // when transaction is part of some block (existing or in production)
    if( _db.is_processing_block() )
    {
      const auto& current_witness = _db.get_witness( gpo.current_witness );
      FC_ASSERT( current_witness.schedule == witness_object::elected, "Subsidized accounts can only be claimed by elected witnesses. current_witness:${w} witness_type:${t}",
        ("w",current_witness.owner)("t",current_witness.schedule) );

      FC_ASSERT( current_witness.available_witness_account_subsidies >= HIVE_ACCOUNT_SUBSIDY_PRECISION, "Witness ${w} does not have enough subsidized accounts to claim",
        ("w", current_witness.owner) );

      _db.modify( current_witness, [&]( witness_object& w )
      {
        w.available_witness_account_subsidies -= HIVE_ACCOUNT_SUBSIDY_PRECISION;
      } );
    }

    FC_ASSERT( gpo.available_account_subsidies >= HIVE_ACCOUNT_SUBSIDY_PRECISION, "There are not enough subsidized accounts to claim" );

    _db.modify( gpo, [&]( dynamic_global_property_object& gpo )
    {
      gpo.available_account_subsidies -= HIVE_ACCOUNT_SUBSIDY_PRECISION;
    } );
  }
  else
  {
    FC_ASSERT( o_fee == wso.median_props.account_creation_fee && "Wrong fee",
      "Must pay the exact account creation fee (or zero if subsidy is to be used). paid: ${p} fee: ${f}",
      ( "p", o_fee )( "f", wso.median_props.account_creation_fee ) );
  }

  _db.adjust_balance( creator, -o_fee );
  _db.adjust_balance( _db.get_account( HIVE_NULL_ACCOUNT ), o_fee );

  _db.modify( creator, [&]( account_object& a )
  {
    a.set_pending_claimed_accounts( a.get_pending_claimed_accounts() + 1 );
  } );
}

void create_claimed_account_evaluator::do_apply( const create_claimed_account_operation& o )
{
  FC_ASSERT( _db.has_hardfork( HIVE_HARDFORK_0_20__1771 ) && "create_claimed_account_operation is not enabled until hardfork 20." );

  const auto& creator = _db.get_account( o.creator );
  const auto& props = _db.get_dynamic_global_properties();

  FC_ASSERT( creator.get_pending_claimed_accounts() > 0, "${creator} has no claimed accounts to create", ( "creator", o.creator ) );

  verify_authority_accounts_exist( _db, o.owner, o.new_account_name, authority::owner );
  verify_authority_accounts_exist( _db, o.active, o.new_account_name, authority::active );
  verify_authority_accounts_exist( _db, o.posting, o.new_account_name, authority::posting );

  _db.modify( creator, [&]( account_object& a )
  {
    a.set_pending_claimed_accounts( a.get_pending_claimed_accounts() - 1 );
  } );

  const auto& new_account = create_account( _db, o.new_account_name, o.memo_key, props.time, _db.get_current_timestamp(),
    false /*mined*/, _db.get_witness_schedule_object().median_props.account_creation_fee, &creator );

  _db.create< account_authority_object >( [&]( account_authority_object& auth )
  {
    auth.account = o.new_account_name;
    auth.owner = o.owner;
    auth.active = o.active;
    auth.posting = o.posting;
    auth.previous_owner_update = fc::time_point_sec::min();
    auth.last_owner_update = fc::time_point_sec::min();
  } );

  push_virtual_operation( _db, account_created_operation( new_account.get_name(), o.creator, VEST_asset( 0 ), VEST_asset( 0 ) ) );
}

void request_account_recovery_evaluator::do_apply( const request_account_recovery_operation& o )
{
  const auto& account_to_recover = _db.get_account( o.account_to_recover );
  const auto& recovery_obj = _db.get< recovery_object, by_account_id >( account_to_recover.get_id() );

  if ( recovery_obj.has_recovery_account() ) // Make sure recovery matches expected recovery account
  {
    const auto& recovery_account = _db.get< account_object, by_id >( recovery_obj.get_recovery_account() );
    FC_ASSERT( recovery_account.get_name() == o.recovery_account, "Cannot recover an account that does not have you as their recovery partner." );
    if( o.recovery_account == HIVE_TEMP_ACCOUNT )
      wlog( "Recovery by temp account" );
  }
  else // Empty recovery account defaults to top witness
  {
    FC_ASSERT( (_db.get_index< witness_index, by_vote_name >().begin()->owner == o.recovery_account), "Top witness must recover an account with no recovery partner." );
  }

  const auto& recovery_request_idx = _db.get_index< account_recovery_request_index, by_account >();
  auto request = recovery_request_idx.find( o.account_to_recover );

  FC_TODO( "validate_auth_size should always be called" );
    //ABW: now there is a bug - editing existing request can introduce authority wider than max,
    //such authority can also be used when cancelling (although it is kind of "workaroundable")
    //HF29 is needed to fix this, after that the check should be moved to validate

  if( request == recovery_request_idx.end() ) // New Request
  {
    FC_ASSERT( !o.new_owner_authority.is_impossible() && "Cannot recover using an impossible authority." );
    FC_ASSERT( o.new_owner_authority.weight_threshold, "Cannot recover using an open authority." );

    validate_auth_size( o.new_owner_authority );

    // Check accounts in the new authority exist
    verify_authority_accounts_exist( _db, o.new_owner_authority, o.account_to_recover, authority::owner );

    _db.create< account_recovery_request_object >( account_to_recover, o.new_owner_authority,
      _db.head_block_time() + HIVE_ACCOUNT_RECOVERY_REQUEST_EXPIRATION_PERIOD );
  }
  else if( o.new_owner_authority.weight_threshold == 0 ) // Cancel Request if authority is open
  {
    _db.remove( *request );
  }
  else // Change Request
  {
    FC_ASSERT( !o.new_owner_authority.is_impossible(), "Cannot recover using an impossible authority." );

    // Check accounts in the new authority exist
    verify_authority_accounts_exist( _db, o.new_owner_authority, o.account_to_recover, authority::owner );

    _db.modify( *request, [&]( account_recovery_request_object& req )
    {
      req.set_new_owner_authority( o.new_owner_authority, _db.head_block_time() + HIVE_ACCOUNT_RECOVERY_REQUEST_EXPIRATION_PERIOD );
    } );
  }
}

void recover_account_evaluator::do_apply( const recover_account_operation& o )
{
  const auto& account = _db.get_account( o.account_to_recover );
  const auto& recovery_obj = _db.get< recovery_object, by_account_id >( account.get_id() );

  if( _db.has_hardfork( HIVE_HARDFORK_0_12 ) )
    FC_ASSERT( util::owner_update_limit_mgr::check( _db.head_block_time(), recovery_obj.get_last_account_recovery_time() ), "${m}", ("m", util::owner_update_limit_mgr::msg( _db.has_hardfork( HIVE_HARDFORK_1_26_AUTH_UPDATE ) ) ) );

  const auto& recovery_request_idx = _db.get_index< account_recovery_request_index, by_account >();
  auto request = recovery_request_idx.find( o.account_to_recover );

  FC_ASSERT( request != recovery_request_idx.end(), "There are no active recovery requests for this account." );
  FC_ASSERT( request->get_new_owner_authority() == o.new_owner_authority, "New owner authority does not match recovery request." );

  const auto& recent_auth_idx = _db.get_index< owner_authority_history_index, by_account >();
  auto hist = recent_auth_idx.lower_bound( o.account_to_recover );
  bool found = false;

  while( hist != recent_auth_idx.end() && hist->account == o.account_to_recover && !found )
  {
    found = hist->previous_owner_authority == o.recent_owner_authority;
    if( found ) break;
    ++hist;
  }

  FC_ASSERT( found, "Recent authority not found in authority history." );

  _db.remove( *request ); // Remove first, update_owner_authority may invalidate iterator
  _db.update_owner_authority( account, o.new_owner_authority );
  _db.modify( recovery_obj, [&]( recovery_object& r )
  {
    r.set_last_account_recovery_time( _db.head_block_time() );
    r.set_block_last_account_recovery_time( _db.get_current_timestamp() );
  });
}

void change_recovery_account_evaluator::do_apply( const change_recovery_account_operation& o )
{
  const auto& new_recovery_account = _db.get_account( o.new_recovery_account ); // validate account exists
    //ABW: can't clear existing recovery agent to set it to top voted witness
  const auto& account_to_recover = _db.get_account( o.account_to_recover );
  const auto& recovery_obj = _db.get< recovery_object, by_account_id >( account_to_recover.get_id() );

  const auto& change_recovery_idx = _db.get_index< change_recovery_account_request_index, by_account >();
  auto request = change_recovery_idx.find( o.account_to_recover );

  if( request == change_recovery_idx.end() ) // New request
  {
    //ABW: it is possible to request change to currently set recovery agent (empty operation)
    _db.create< change_recovery_account_request_object >( account_to_recover, new_recovery_account, _db.head_block_time() + HIVE_OWNER_AUTH_RECOVERY_PERIOD );
  }
  else if( recovery_obj.get_recovery_account() != new_recovery_account.get_id() ) // Change existing request
  {
    //ABW: it is possible to request change to already requested new recovery agent (operation only resets timer)
    _db.modify( *request, [&]( change_recovery_account_request_object& req )
    {
      req.set_recovery_account( new_recovery_account, _db.head_block_time() + HIVE_OWNER_AUTH_RECOVERY_PERIOD );
    });
  }
  else // Request exists and changing back to current recovery account
  {
    _db.remove( *request );
  }
}

void decline_voting_rights_evaluator::do_apply( const decline_voting_rights_operation& o )
{
  FC_ASSERT( _db.has_hardfork( HIVE_HARDFORK_0_14__324 ) );

  const auto& account = _db.get_account( o.account );

  if( _db.is_in_control() || _db.has_hardfork( HIVE_HARDFORK_1_28 ) )
    FC_ASSERT( account.can_vote() && "Voter declined voting rights already, therefore trying to decline voting rights again is forbidden." );

  const auto& request_idx = _db.get_index< decline_voting_rights_request_index >().indices().get< by_account >();
  auto itr = request_idx.find( account.get_name() );

  if( o.decline )
  {
    FC_ASSERT( itr == request_idx.end(), "Cannot create new request because one already exists." );

    _db.create< decline_voting_rights_request_object >( [&]( decline_voting_rights_request_object& req )
    {
      req.account = account.get_name();
      req.effective_date = _db.head_block_time() + HIVE_OWNER_AUTH_RECOVERY_PERIOD;
    });
  }
  else
  {
    FC_ASSERT( itr != request_idx.end(), "Cannot cancel the request because it does not exist." );
    _db.remove( *itr );
  }
}

void reset_account_evaluator::do_apply( const reset_account_operation& op )
{
  FC_ASSERT( false && "Reset Account Operation is currently disabled." );
  //ABW: see discussion in https://github.com/steemit/steem/issues/240
  //apparently the idea was never put in active use and it does not seem it ever will
  //related member of account_object was removed as it was taking space with no purpose
}

void set_reset_account_evaluator::do_apply( const set_reset_account_operation& op )
{
  FC_ASSERT( false && "Set Reset Account Operation is currently disabled." );
  //related to reset_account_operation
}

} } // hive::chain
