#include <hive/chain/hive_fwd.hpp>

#include <hive/protocol/fixed_string.hpp>

#include <hive/chain/hive_evaluator.hpp>
#include <hive/chain/database.hpp>
#include <hive/chain/custom_operation_interpreter.hpp>
#include <hive/chain/hive_objects.hpp>
#include <hive/chain/witness_objects.hpp>
#include <hive/chain/block_summary_object.hpp>

#include <hive/chain/util/reward.hpp>
#include <hive/chain/util/manabar.hpp>
#include <hive/chain/util/delayed_voting.hpp>
#include <hive/chain/util/owner_update_limit_mgr.hpp>

#include <fc/macros.hpp>

#include <fc/uint128.hpp>
#include <fc/utf8.hpp>

#include <boost/scope_exit.hpp>

#include <limits>

namespace hive { namespace chain {
  using fc::uint128_t;

inline void validate_permlink_0_1( const string& permlink )
{
  FC_ASSERT( permlink.size() > HIVE_MIN_PERMLINK_LENGTH && permlink.size() < HIVE_MAX_PERMLINK_LENGTH, "Permlink is not a valid size." );

  for( const auto& c : permlink )
  {
    switch( c )
    {
      case 'a': case 'b': case 'c': case 'd': case 'e': case 'f': case 'g': case 'h': case 'i':
      case 'j': case 'k': case 'l': case 'm': case 'n': case 'o': case 'p': case 'q': case 'r':
      case 's': case 't': case 'u': case 'v': case 'w': case 'x': case 'y': case 'z': case '0':
      case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
      case '-':
        break;
      default:
        FC_ASSERT( false, "Invalid permlink character: ${s}", ("s", std::string() + c ) );
    }
  }
}

template< bool force_canon >
void copy_legacy_chain_properties( chain_properties& dest, const legacy_chain_properties& src )
{
  dest.account_creation_fee = src.account_creation_fee.to_asset< force_canon >();
  dest.maximum_block_size = src.maximum_block_size;
  dest.hbd_interest_rate = src.hbd_interest_rate;
}

void witness_update_evaluator::do_apply( const witness_update_operation& o )
{
  _db.get_account( o.owner ); // verify owner exists

  if ( _db.has_hardfork( HIVE_HARDFORK_0_14__410 ) )
  {
    FC_ASSERT( o.props.account_creation_fee.symbol.is_canon() );
    if( _db.has_hardfork( HIVE_HARDFORK_0_20__2651 ) )
    {
      FC_TODO( "Move to validate() after HF20" );
      FC_ASSERT( o.props.account_creation_fee.amount <= HIVE_MAX_ACCOUNT_CREATION_FEE, "account_creation_fee greater than maximum account creation fee" );
    }
  }
  else if( !o.props.account_creation_fee.symbol.is_canon() )
  {
    // after HF, above check can be moved to validate() if reindex doesn't show this warning
    _db.push_virtual_operation( system_warning_operation( FC_LOG_MESSAGE( warn,
      "Wrong fee symbol in block ${b}", ( "b", _db.head_block_num() + 1 ) ).get_message() ) );
  }

  FC_TODO( "Check and move this to validate after HF 20" );
  if( _db.has_hardfork( HIVE_HARDFORK_0_20__2642 ) )
  {
    FC_ASSERT( o.props.maximum_block_size <= HIVE_MAX_BLOCK_SIZE, "Max block size cannot be more than 2MiB" );
  }

  const auto& by_witness_name_idx = _db.get_index< witness_index >().indices().get< by_name >();
  auto wit_itr = by_witness_name_idx.find( o.owner );
  if( wit_itr != by_witness_name_idx.end() )
  {
    _db.modify( *wit_itr, [&]( witness_object& w ) {
      from_string( w.url, o.url );
      w.signing_key        = o.block_signing_key;
      copy_legacy_chain_properties< false >( w.props, o.props );
    });
  }
  else
  {
    _db.create< witness_object >( [&]( witness_object& w ) {
      w.owner              = o.owner;
      from_string( w.url, o.url );
      w.signing_key        = o.block_signing_key;
      w.created            = _db.head_block_time();
      copy_legacy_chain_properties< false >( w.props, o.props );
    });
  }
}

struct witness_properties_change_flags
{
  uint32_t account_creation_changed       : 1;
  uint32_t max_block_changed              : 1;
  uint32_t hbd_interest_changed           : 1;
  uint32_t account_subsidy_budget_changed : 1;
  uint32_t account_subsidy_decay_changed  : 1;
  uint32_t key_changed                    : 1;
  uint32_t hbd_exchange_changed           : 1;
  uint32_t url_changed                    : 1;
};

void witness_set_properties_evaluator::do_apply( const witness_set_properties_operation& o )
{
  FC_ASSERT( _db.has_hardfork( HIVE_HARDFORK_0_20__1620 ), "witness_set_properties_evaluator not enabled until HF 20" );

  const auto& witness = _db.get< witness_object, by_name >( o.owner ); // verifies witness exists;

  // Capture old properties. This allows only updating the object once.
  chain_properties  props;
  public_key_type   signing_key;
  price             hbd_exchange_rate;
  time_point_sec    last_hbd_exchange_update;
  string            url;

  witness_properties_change_flags flags;

  auto itr = o.props.find( "key" );

  // This existence of 'key' is checked in witness_set_properties_operation::validate
  fc::raw::unpack_from_vector( itr->second, signing_key );
  FC_ASSERT( signing_key == witness.signing_key, "'key' does not match witness signing key.",
    ("key", signing_key)("signing_key", witness.signing_key) );

  itr = o.props.find( "account_creation_fee" );
  flags.account_creation_changed = itr != o.props.end();
  if( flags.account_creation_changed )
  {
    fc::raw::unpack_from_vector( itr->second, props.account_creation_fee );
    if( _db.has_hardfork( HIVE_HARDFORK_0_20__2651 ) )
    {
      FC_TODO( "Move to validate() after HF20" );
      FC_ASSERT( props.account_creation_fee.amount <= HIVE_MAX_ACCOUNT_CREATION_FEE, "account_creation_fee greater than maximum account creation fee" );
    }
  }

  itr = o.props.find( "maximum_block_size" );
  flags.max_block_changed = itr != o.props.end();
  if( flags.max_block_changed )
  {
    fc::raw::unpack_from_vector( itr->second, props.maximum_block_size );
  }

  itr = o.props.find( "sbd_interest_rate" );
  if(itr == o.props.end() && _db.has_hardfork(HIVE_HARDFORK_1_24))
    itr = o.props.find( "hbd_interest_rate" );

  flags.hbd_interest_changed = itr != o.props.end();
  if( flags.hbd_interest_changed )
  {
    fc::raw::unpack_from_vector( itr->second, props.hbd_interest_rate );
  }

  itr = o.props.find( "account_subsidy_budget" );
  flags.account_subsidy_budget_changed = itr != o.props.end();
  if( flags.account_subsidy_budget_changed )
  {
    fc::raw::unpack_from_vector( itr->second, props.account_subsidy_budget );
  }

  itr = o.props.find( "account_subsidy_decay" );
  flags.account_subsidy_decay_changed = itr != o.props.end();
  if( flags.account_subsidy_decay_changed )
  {
    fc::raw::unpack_from_vector( itr->second, props.account_subsidy_decay );
  }

  itr = o.props.find( "new_signing_key" );
  flags.key_changed = itr != o.props.end();
  if( flags.key_changed )
  {
    fc::raw::unpack_from_vector( itr->second, signing_key );
  }

  itr = o.props.find( "sbd_exchange_rate" );
  if(itr == o.props.end() && _db.has_hardfork(HIVE_HARDFORK_1_24))
    itr = o.props.find("hbd_exchange_rate");

  flags.hbd_exchange_changed = itr != o.props.end();
  if( flags.hbd_exchange_changed )
  {
    fc::raw::unpack_from_vector( itr->second, hbd_exchange_rate );
    last_hbd_exchange_update = _db.head_block_time();
  }

  itr = o.props.find( "url" );
  flags.url_changed = itr != o.props.end();
  if( flags.url_changed )
  {
    fc::raw::unpack_from_vector< std::string >( itr->second, url );
  }

  _db.modify( witness, [&]( witness_object& w )
  {
    if( flags.account_creation_changed )
    {
      w.props.account_creation_fee = props.account_creation_fee;
    }

    if( flags.max_block_changed )
    {
      w.props.maximum_block_size = props.maximum_block_size;
    }

    if( flags.hbd_interest_changed )
    {
      w.props.hbd_interest_rate = props.hbd_interest_rate;
    }

    if( flags.account_subsidy_budget_changed )
    {
      w.props.account_subsidy_budget = props.account_subsidy_budget;
    }

    if( flags.account_subsidy_decay_changed )
    {
      w.props.account_subsidy_decay = props.account_subsidy_decay;
    }

    if( flags.key_changed )
    {
      w.signing_key = signing_key;
    }

    if( flags.hbd_exchange_changed )
    {
      w.hbd_exchange_rate = hbd_exchange_rate;
      w.last_hbd_exchange_update = last_hbd_exchange_update;
    }

    if( flags.url_changed )
    {
      from_string( w.url, url );
    }
  });
}

void verify_authority_accounts_exist(
  const database& db,
  const authority& auth,
  const account_name_type& auth_account,
  authority::classification auth_class)
{
  for( const std::pair< account_name_type, weight_type >& aw : auth.account_auths )
  {
    const account_object* a = db.find_account( aw.first );
    FC_ASSERT( a != nullptr, "New ${ac} authority on account ${aa} references non-existing account ${aref}",
      ("aref", aw.first)("ac", auth_class)("aa", auth_account) );
  }
}

const account_object& create_account( database& db, const account_name_type& name, const public_key_type& key,
  const time_point_sec& time, bool mined, const account_object* recovery_account = nullptr, asset initial_delegation = asset( 0, VESTS_SYMBOL ) )
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

  return db.create< account_object >( name, key, time, mined, recovery_account,
    !db.has_hardfork( HIVE_HARDFORK_0_20__2539 ) /*voting mana 100%*/, initial_delegation );
}

void account_create_evaluator::do_apply( const account_create_operation& o )
{
  const auto& creator = _db.get_account( o.creator );

  const auto& props = _db.get_dynamic_global_properties();

  const witness_schedule_object& wso = _db.get_witness_schedule_object();

  if( _db.has_hardfork( HIVE_HARDFORK_0_20__2651 ) )
  {
    FC_TODO( "Move to validate() after HF20" );
    FC_ASSERT( o.fee <= asset( HIVE_MAX_ACCOUNT_CREATION_FEE, HIVE_SYMBOL ), "Account creation fee cannot be too large" );
  }

  if( _db.has_hardfork( HIVE_HARDFORK_0_20__1771 ) )
  {
    FC_ASSERT( o.fee == wso.median_props.account_creation_fee, "Must pay the exact account creation fee. paid: ${p} fee: ${f}",
            ("p", o.fee)
            ("f", wso.median_props.account_creation_fee) );
  }
  else if( !_db.has_hardfork( HIVE_HARDFORK_0_20__1761 ) && _db.has_hardfork( HIVE_HARDFORK_0_19__987 ) )
  {
    FC_ASSERT( o.fee >= asset( wso.median_props.account_creation_fee.amount * HIVE_CREATE_ACCOUNT_WITH_HIVE_MODIFIER, HIVE_SYMBOL ), "Insufficient Fee: ${f} required, ${p} provided.",
            ("f", asset( wso.median_props.account_creation_fee.amount * HIVE_CREATE_ACCOUNT_WITH_HIVE_MODIFIER, HIVE_SYMBOL ))
            ("p", o.fee) );
  }
  else if( _db.has_hardfork( HIVE_HARDFORK_0_1 ) )
  {
    FC_ASSERT( o.fee >= wso.median_props.account_creation_fee, "Insufficient Fee: ${f} required, ${p} provided.",
            ("f", wso.median_props.account_creation_fee)
            ("p", o.fee) );
  }

  FC_TODO( "Check and move to validate post HF20" );
  if( _db.has_hardfork( HIVE_HARDFORK_0_20 ) )
  {
    validate_auth_size( o.owner );
    validate_auth_size( o.active );
    validate_auth_size( o.posting );
  }

  if( _db.has_hardfork( HIVE_HARDFORK_0_15__465 ) )
  {
    verify_authority_accounts_exist( _db, o.owner, o.new_account_name, authority::owner );
    verify_authority_accounts_exist( _db, o.active, o.new_account_name, authority::active );
    verify_authority_accounts_exist( _db, o.posting, o.new_account_name, authority::posting );
  }

  _db.adjust_balance( creator, -o.fee );

  if( _db.has_hardfork( HIVE_HARDFORK_0_20__1762 ) )
  {
    _db.adjust_balance( _db.get< account_object, by_name >( HIVE_NULL_ACCOUNT ), o.fee );
  }

  const auto& new_account = create_account( _db, o.new_account_name, o.memo_key, props.time, false /*mined*/, &creator );

#ifdef COLLECT_ACCOUNT_METADATA
  _db.create< account_metadata_object >( [&]( account_metadata_object& meta )
  {
    meta.account = new_account.get_id();
    from_string( meta.json_metadata, o.json_metadata );
  });
#else
  FC_UNUSED( new_account );
#endif

  _db.create< account_authority_object >( [&]( account_authority_object& auth )
  {
    auth.account = o.new_account_name;
    auth.owner = o.owner;
    auth.active = o.active;
    auth.posting = o.posting;
    auth.previous_owner_update = fc::time_point_sec::min();
    auth.last_owner_update = fc::time_point_sec::min();
  });

  asset initial_vesting_shares;
  if( !_db.has_hardfork( HIVE_HARDFORK_0_20__1762 ) && o.fee.amount > 0 )
  {
    initial_vesting_shares = _db.create_vesting( new_account, o.fee );
  }
  else
  {
    initial_vesting_shares = asset(0, VESTS_SYMBOL);
  }
  _db.push_virtual_operation( account_created_operation(o.new_account_name, o.creator, initial_vesting_shares, asset(0, VESTS_SYMBOL) ) );
}

void account_create_with_delegation_evaluator::do_apply( const account_create_with_delegation_operation& o )
{
  FC_ASSERT( !_db.has_hardfork( HIVE_HARDFORK_0_20__1760 ), "Account creation with delegation is deprecated as of Hardfork 20" );

  if( _db.has_hardfork( HIVE_HARDFORK_0_20__2651 ) )
  {
    FC_TODO( "Move to validate() after HF20" );
    FC_ASSERT( o.fee <= asset( HIVE_MAX_ACCOUNT_CREATION_FEE, HIVE_SYMBOL ), "Account creation fee cannot be too large" );
  }

  const auto& creator = _db.get_account( o.creator );
  const auto& props = _db.get_dynamic_global_properties();
  const witness_schedule_object& wso = _db.get_witness_schedule_object();

  FC_ASSERT( creator.get_balance() >= o.fee, "Insufficient balance to create account.",
          ( "creator.balance", creator.get_balance() )
          ( "required", o.fee ) );

  FC_ASSERT( static_cast<asset>(creator.get_vesting()) - creator.delegated_vesting_shares - asset( creator.to_withdraw.amount - creator.withdrawn.amount, VESTS_SYMBOL ) >= o.delegation, "Insufficient vesting shares to delegate to new account.",
          ( "creator.vesting_shares", creator.get_vesting() )
          ( "creator.delegated_vesting_shares", creator.delegated_vesting_shares )( "required", o.delegation ) );

  auto target_delegation = asset( wso.median_props.account_creation_fee.amount * HIVE_CREATE_ACCOUNT_WITH_HIVE_MODIFIER * HIVE_CREATE_ACCOUNT_DELEGATION_RATIO, HIVE_SYMBOL ) * props.get_vesting_share_price();

  auto current_delegation = asset( o.fee.amount * HIVE_CREATE_ACCOUNT_DELEGATION_RATIO, HIVE_SYMBOL ) * props.get_vesting_share_price() + o.delegation;

  FC_ASSERT( current_delegation >= target_delegation, "Insufficient Delegation ${f} required, ${p} provided.",
          ("f", target_delegation )
          ( "p", current_delegation )
          ( "account_creation_fee", wso.median_props.account_creation_fee )
          ( "o.fee", o.fee )
          ( "o.delegation", o.delegation ) );

  FC_ASSERT( o.fee >= wso.median_props.account_creation_fee, "Insufficient Fee: ${f} required, ${p} provided.",
          ("f", wso.median_props.account_creation_fee)
          ("p", o.fee) );

  FC_TODO( "Check and move to validate post HF20" );
  if( _db.has_hardfork( HIVE_HARDFORK_0_20 ) )
  {
    validate_auth_size( o.owner );
    validate_auth_size( o.active );
    validate_auth_size( o.posting );
  }

  for( const auto& a : o.owner.account_auths )
  {
    _db.get_account( a.first );
  }

  for( const auto& a : o.active.account_auths )
  {
    _db.get_account( a.first );
  }

  for( const auto& a : o.posting.account_auths )
  {
    _db.get_account( a.first );
  }

  _db.modify( creator, [&]( account_object& c )
  {
    c.balance -= o.fee;
    c.delegated_vesting_shares += o.delegation;
  });

  if( _db.has_hardfork( HIVE_HARDFORK_0_20__1762 ) )
  {
    _db.adjust_balance( _db.get< account_object, by_name >( HIVE_NULL_ACCOUNT ), o.fee );
  }

  const auto& new_account = create_account( _db, o.new_account_name, o.memo_key, props.time, false /*mined*/, &creator, o.delegation );

#ifdef COLLECT_ACCOUNT_METADATA
  _db.create< account_metadata_object >( [&]( account_metadata_object& meta )
  {
    meta.account = new_account.get_id();
    from_string( meta.json_metadata, o.json_metadata );
  });
#else
  FC_UNUSED( new_account );
#endif

  _db.create< account_authority_object >( [&]( account_authority_object& auth )
  {
    auth.account = o.new_account_name;
    auth.owner = o.owner;
    auth.active = o.active;
    auth.posting = o.posting;
    auth.previous_owner_update = fc::time_point_sec::min();
    auth.last_owner_update = fc::time_point_sec::min();
  });

  if( o.delegation.amount > 0 || !_db.has_hardfork( HIVE_HARDFORK_0_19__997 ) )
  {
    //ABW: for future reference:
    //the above HF19 condition cannot be eliminated, because when delegation is created here, its minimal time
    //is 30 days, while delegation created normally will have no effective limit;
    //this means that there is difference between zero delegation created here and then increased with
    //delegate_vesting_shares_operation and delegation created with delegate_vesting_shares_operation;
    //if such delegation is then reduced, delegated VESTs will return to delegator at different time, in case
    //30 days after creation happens to be later than "now - dgpo.delegation_return_period" at the time when
    //delegation is reduced; such situation actually happened
    _db.create< vesting_delegation_object >( creator, new_account, o.delegation,
      _db.head_block_time() + HIVE_CREATE_ACCOUNT_DELEGATION_TIME );
  }

  asset initial_vesting_shares;
  if( !_db.has_hardfork( HIVE_HARDFORK_0_20__1762 ) && o.fee.amount > 0 )
  {
    initial_vesting_shares = _db.create_vesting( new_account, o.fee );
  }
  else
  {
    initial_vesting_shares = asset(0, VESTS_SYMBOL);
  }
  _db.push_virtual_operation( account_created_operation( o.new_account_name, o.creator, initial_vesting_shares, o.delegation) );
}


void account_update_evaluator::do_apply( const account_update_operation& o )
{
  if( _db.has_hardfork( HIVE_HARDFORK_0_1 ) ) FC_ASSERT( o.account != HIVE_TEMP_ACCOUNT, "Cannot update temp account." );

  if( ( _db.has_hardfork( HIVE_HARDFORK_0_15__465 ) ) && o.posting )
    o.posting->validate();

  const auto& account = _db.get_account( o.account );
  const auto& account_auth = _db.get< account_authority_object, by_account >( o.account );

  if( _db.has_hardfork( HIVE_HARDFORK_0_20 ) )
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
                                                      account_auth.previous_owner_update, account_auth.last_owner_update ), "${m}", ("m", util::owner_update_limit_mgr::msg( _db.has_hardfork( HIVE_HARDFORK_1_26_AUTH_UPDATE ) )) );
# endif

    if( ( _db.has_hardfork( HIVE_HARDFORK_0_15__465 ) ) )
      verify_authority_accounts_exist( _db, *o.owner, o.account, authority::owner );

    _db.update_owner_authority( account, *o.owner );
  }
  if( o.active && ( _db.has_hardfork( HIVE_HARDFORK_0_15__465 ) ) )
    verify_authority_accounts_exist( _db, *o.active, o.account, authority::active );
  if( o.posting && ( _db.has_hardfork( HIVE_HARDFORK_0_15__465 ) ) )
    verify_authority_accounts_exist( _db, *o.posting, o.account, authority::posting );

  _db.modify( account, [&]( account_object& acc )
  {
    if( o.memo_key != public_key_type() )
        acc.memo_key = o.memo_key;

    acc.last_account_update = _db.head_block_time();
  });

  #ifdef COLLECT_ACCOUNT_METADATA
  if( o.json_metadata.size() > 0 )
  {
    _db.modify( _db.get< account_metadata_object, by_account >( account.get_id() ), [&]( account_metadata_object& meta )
    {
      from_string( meta.json_metadata, o.json_metadata );
      if ( !_db.has_hardfork( HIVE_HARDFORK_0_21__3274 ) )
      {
        from_string( meta.posting_json_metadata, o.json_metadata );
      }
    });
  }
  #endif

  if( o.active || o.posting )
  {
    _db.modify( account_auth, [&]( account_authority_object& auth)
    {
      if( o.active )  auth.active  = *o.active;
      if( o.posting ) auth.posting = *o.posting;
    });
  }

}

void account_update2_evaluator::do_apply( const account_update2_operation& o )
{
  FC_ASSERT( _db.has_hardfork( HIVE_HARDFORK_0_21__3274 ), "Operation 'account_update2' is not enabled until HF 21" );
  FC_ASSERT( o.account != HIVE_TEMP_ACCOUNT, "Cannot update temp account." );

  if( o.posting )
    o.posting->validate();

  const auto& account = _db.get_account( o.account );
  const auto& account_auth = _db.get< account_authority_object, by_account >( o.account );

  if( o.owner )
    validate_auth_size( *o.owner );
  if( o.active )
    validate_auth_size( *o.active );
  if( o.posting )
    validate_auth_size( *o.posting );

  if( o.owner )
  {
    FC_ASSERT( util::owner_update_limit_mgr::check( _db.has_hardfork( HIVE_HARDFORK_1_26_AUTH_UPDATE ), _db.head_block_time(),
                                                    account_auth.previous_owner_update, account_auth.last_owner_update ), "${m}", ("m", util::owner_update_limit_mgr::msg( _db.has_hardfork( HIVE_HARDFORK_1_26_AUTH_UPDATE ) ) ) );

    verify_authority_accounts_exist( _db, *o.owner, o.account, authority::owner );

    _db.update_owner_authority( account, *o.owner );
  }
  if( o.active )
    verify_authority_accounts_exist( _db, *o.active, o.account, authority::active );
  if( o.posting )
    verify_authority_accounts_exist( _db, *o.posting, o.account, authority::posting );

  _db.modify( account, [&]( account_object& acc )
  {
    if( o.memo_key && *o.memo_key != public_key_type() )
        acc.memo_key = *o.memo_key;

    acc.last_account_update = _db.head_block_time();
  });

  #ifdef COLLECT_ACCOUNT_METADATA
  if( o.json_metadata.size() > 0 || o.posting_json_metadata.size() > 0 )
  {
    _db.modify( _db.get< account_metadata_object, by_account >( account.get_id() ), [&]( account_metadata_object& meta )
    {
      if ( o.json_metadata.size() > 0 )
        from_string( meta.json_metadata, o.json_metadata );

      if ( o.posting_json_metadata.size() > 0 )
        from_string( meta.posting_json_metadata, o.posting_json_metadata );
    });
  }
  #endif

  if( o.active || o.posting )
  {
    _db.modify( account_auth, [&]( account_authority_object& auth)
    {
      if( o.active )  auth.active  = *o.active;
      if( o.posting ) auth.posting = *o.posting;
    });
  }

}

/**
  *  Because net_rshares is 0 there is no need to update any pending payout calculations or parent posts.
  */
void delete_comment_evaluator::do_apply( const delete_comment_operation& o )
{
  const auto& comment = _db.get_comment( o.author, o.permlink );
  const comment_cashout_object* comment_cashout = _db.find_comment_cashout( comment );
  if( comment_cashout )
    FC_ASSERT( !comment_cashout->has_replies(), "Cannot delete a comment with replies." );

  //if( _db.has_hardfork( HIVE_HARDFORK_0_19__876 ) )
  /*
    Before `HF19`: `comment_cashout` exists for sure -  following assertion always is true
    After  `HF19`: when `comment_cashout` doesn't exist( it's possible ), then assertion should be triggered
  */
  FC_ASSERT( comment_cashout, "Cannot delete comment after payout." );

  if( _db.has_hardfork( HIVE_HARDFORK_0_19__977 ) )
    FC_ASSERT( comment_cashout->get_net_rshares() <= 0, "Cannot delete a comment with net positive votes." );

  if( comment_cashout->get_net_rshares() > 0 )
  {
    _db.push_virtual_operation( ineffective_delete_comment_operation( o.author, o.permlink ) );
    return;
  }

  const auto& vote_idx = _db.get_index<comment_vote_index, by_comment_voter>();

  auto vote_itr = vote_idx.lower_bound( comment.get_id() );
  while( vote_itr != vote_idx.end() && vote_itr->get_comment() == comment.get_id() )
  {
    const auto& cur_vote = *vote_itr;
    ++vote_itr;
    _db.remove(cur_vote);
  }

  if( !comment.is_root() )
  {
    const comment_cashout_object* parent = _db.find_comment_cashout( _db.get_comment( comment.get_parent_id() ) );
    if( parent )
    {
      _db.modify( *parent, [&]( comment_cashout_object& p )
      {
        p.on_reply( _db.head_block_time(), true );
      } );
    }
  }

  if( !_db.has_hardfork( HIVE_HARDFORK_0_19 ) )
  {
    const auto* c_ex = _db.find_comment_cashout_ex( comment );
    _db.remove( *c_ex );
  }
  _db.remove( *comment_cashout );
  _db.remove( comment );
}

struct comment_options_extension_visitor
{
  comment_options_extension_visitor( const comment_cashout_object& c, database& db ) : _c( c ), _db( db ) {}

  typedef void result_type;

  const comment_cashout_object& _c;
  database& _db;

#ifdef HIVE_ENABLE_SMT
  void operator()( const allowed_vote_assets& va) const
  {
    FC_ASSERT( !_c.has_votes(), "Comment must not have been voted on before specifying allowed vote assets." );
    auto remaining_asset_number = SMT_MAX_VOTABLE_ASSETS;
    FC_ASSERT( remaining_asset_number > 0 );
    _db.modify( _c, [&]( comment_cashout_object& c )
    {
      for( const auto& a : va.votable_assets )
      {
        if( a.first != HIVE_SYMBOL )
        {
          FC_ASSERT( remaining_asset_number > 0, "Comment votable assets number exceeds allowed limit ${ava}.",
                ("ava", SMT_MAX_VOTABLE_ASSETS) );
          --remaining_asset_number;
          c.allowed_vote_assets.emplace_back( a.first, a.second );
        }
      }
    });
  }
#endif

  void operator()( const comment_payout_beneficiaries& cpb ) const
  {
    FC_ASSERT( _c.get_beneficiaries().size() == 0, "Comment already has beneficiaries specified." );
    FC_ASSERT( !_c.has_votes(), "Comment must not have been voted on before specifying beneficiaries." );

    _db.modify( _c, [&]( comment_cashout_object& c )
    {
      for( auto& b : cpb.beneficiaries )
      {
        auto acc = _db.find< account_object, by_name >( b.account );
        FC_ASSERT( acc != nullptr, "Beneficiary \"${a}\" must exist.", ("a", b.account) );
        c.add_beneficiary( *acc, b.weight );
      }
    });
  }
};

void comment_options_evaluator::do_apply( const comment_options_operation& o )
{
  const auto& comment = _db.get_comment( o.author, o.permlink );

  const comment_cashout_object* comment_cashout = _db.find_comment_cashout( comment );

  /*
    If `comment_cashout` doesn't exist then setting members needed for payout is not necessary
    Further operations can be skipped
  */
  if( !comment_cashout )
  {
    FC_ASSERT( !_db.has_hardfork( HIVE_HARDFORK_1_24 ), "Updating parameters for comment that is paid out is forbidden." );
    return;
  }

  if( !o.allow_curation_rewards || !o.allow_votes || o.max_accepted_payout < comment_cashout->get_max_accepted_payout() )
    FC_ASSERT( !comment_cashout->has_votes(), "One of the included comment options requires the comment to have no rshares allocated to it." );

  FC_ASSERT( comment_cashout->allows_curation_rewards() >= o.allow_curation_rewards, "Curation rewards cannot be re-enabled." );
  FC_ASSERT( comment_cashout->allows_votes() >= o.allow_votes, "Voting cannot be re-enabled." );
  FC_ASSERT( comment_cashout->get_max_accepted_payout() >= o.max_accepted_payout, "A comment cannot accept a greater payout." );
  FC_ASSERT( comment_cashout->get_percent_hbd() >= o.percent_hbd, "A comment cannot accept a greater percent HBD." );

  _db.modify( *comment_cashout, [&]( comment_cashout_object& c )
  {
    c.configure_options( o.percent_hbd, o.max_accepted_payout, o.allow_votes, o.allow_curation_rewards );
  });

  for( auto& e : o.extensions )
  {
    e.visit( comment_options_extension_visitor( *comment_cashout, _db ) );
  }
}

void comment_evaluator::do_apply( const comment_operation& o )
{ try {
  if( _db.has_hardfork( HIVE_HARDFORK_0_5__55 ) )
    FC_ASSERT( o.title.size() + o.body.size() + o.json_metadata.size(), "Cannot update comment because nothing appears to be changing." );

  const auto& auth = _db.get_account( o.author ); /// prove it exists

  const auto& by_permlink_idx = _db.get_index< comment_index >().indices().get< by_permlink >();
  auto itr = by_permlink_idx.find( comment_object::compute_author_and_permlink_hash( auth.get_id(), o.permlink ) );
  auto _now = _db.head_block_time();

  const comment_object* parent = nullptr;
  if( o.parent_author != HIVE_ROOT_POST_PARENT )
  {
    parent = &_db.get_comment( o.parent_author, o.parent_permlink );
    if( !_db.has_hardfork( HIVE_HARDFORK_0_17__767 ) )
      FC_ASSERT( parent->get_depth() < HIVE_MAX_COMMENT_DEPTH_PRE_HF17, "Comment is nested ${x} posts deep, maximum depth is ${y}.", ("x",parent->get_depth())("y",HIVE_MAX_COMMENT_DEPTH_PRE_HF17) );
    else
      FC_ASSERT( parent->get_depth() < HIVE_MAX_COMMENT_DEPTH, "Comment is nested ${x} posts deep, maximum depth is ${y}.", ("x",parent->get_depth())("y",HIVE_MAX_COMMENT_DEPTH) );
  }

  FC_ASSERT( fc::is_utf8( o.json_metadata ), "JSON Metadata must be UTF-8" );

  if ( itr == by_permlink_idx.end() )
  {
    if( parent )
    {
      if( _db.has_hardfork( HIVE_HARDFORK_0_12__177 ) && !_db.has_hardfork( HIVE_HARDFORK_0_17__869 ) )
        FC_ASSERT( _db.calculate_discussion_payout_time( *parent ) != fc::time_point_sec::maximum(), "Discussion is frozen." );
    }

    FC_TODO( "Cleanup this logic after HF 20. Old ops don't need to check pre-hf20 times." )
    if( _db.has_hardfork( HIVE_HARDFORK_0_20__2019 ) )
    {
      if( !parent )
        FC_ASSERT( ( _now - auth.last_root_post ) > HIVE_MIN_ROOT_COMMENT_INTERVAL, "You may only post once every 5 minutes.", ("now",_now)("last_root_post", auth.last_root_post) );
      else
        FC_ASSERT( ( _now - auth.last_post ) >= HIVE_MIN_REPLY_INTERVAL_HF20, "You may only comment once every 3 seconds.", ("now",_now)("auth.last_post",auth.last_post) );
    }
    else if( _db.has_hardfork( HIVE_HARDFORK_0_12__176 ) )
    {
      if( !parent )
        FC_ASSERT( ( _now - auth.last_root_post ) > HIVE_MIN_ROOT_COMMENT_INTERVAL, "You may only post once every 5 minutes.", ("now",_now)("last_root_post", auth.last_root_post) );
      else
        FC_ASSERT( ( _now - auth.last_post ) > HIVE_MIN_REPLY_INTERVAL, "You may only comment once every 20 seconds.", ("now",_now)("auth.last_post",auth.last_post) );
    }
    else if( _db.has_hardfork( HIVE_HARDFORK_0_6__113 ) )
    {
      if( !parent )
        FC_ASSERT( ( _now - auth.last_post ) > HIVE_MIN_ROOT_COMMENT_INTERVAL, "You may only post once every 5 minutes.", ("now",_now)("auth.last_post",auth.last_post) );
      else
        FC_ASSERT( ( _now - auth.last_post ) > HIVE_MIN_REPLY_INTERVAL, "You may only comment once every 20 seconds.", ("now",_now)("auth.last_post",auth.last_post) );
    }
    else
    {
      FC_ASSERT( ( _now - auth.last_post ) > fc::seconds(60), "You may only post once per minute.", ("now",_now)("auth.last_post",auth.last_post) );
    }

    uint16_t reward_weight = HIVE_100_PERCENT;
    uint64_t post_bandwidth = auth.post_bandwidth;

    if( _db.has_hardfork( HIVE_HARDFORK_0_12__176 ) && !_db.has_hardfork( HIVE_HARDFORK_0_17__733 ) && !parent )
    {
      uint64_t post_delta_time = std::min( _now.sec_since_epoch() - auth.last_root_post.sec_since_epoch(), HIVE_POST_AVERAGE_WINDOW );
      uint32_t old_weight = uint32_t( ( post_bandwidth * ( HIVE_POST_AVERAGE_WINDOW - post_delta_time ) ) / HIVE_POST_AVERAGE_WINDOW );
      post_bandwidth = ( old_weight + HIVE_100_PERCENT );
      reward_weight = uint16_t( std::min( ( HIVE_POST_WEIGHT_CONSTANT * HIVE_100_PERCENT ) / ( post_bandwidth * post_bandwidth ), uint64_t( HIVE_100_PERCENT ) ) );
    }

    _db.modify( auth, [&]( account_object& a )
    {
      if( !parent )
      {
        a.last_root_post = _now;
        a.post_bandwidth = uint32_t( post_bandwidth );
      }
      a.last_post = _now;
      a.last_post_edit = _now;
      a.post_count++;
    });

    if( _db.has_hardfork( HIVE_HARDFORK_0_1 ) )
    {
      validate_permlink_0_1( o.parent_permlink );
      validate_permlink_0_1( o.permlink );
    }

    const auto& new_comment = _db.create< comment_object >( auth, o.permlink, parent );

    fc::time_point_sec cashout_time;
    if( _db.has_hardfork( HIVE_HARDFORK_0_17__769 ) )
      cashout_time = _now + HIVE_CASHOUT_WINDOW_SECONDS;
    else if( !parent && _db.has_hardfork( HIVE_HARDFORK_0_12__177 ) )
      cashout_time = _now + HIVE_CASHOUT_WINDOW_SECONDS_PRE_HF17;
    else
      cashout_time = fc::time_point_sec::maximum();

    _db.create< comment_cashout_object >( new_comment, auth, o.permlink, _now, cashout_time );
    if( !_db.has_hardfork( HIVE_HARDFORK_0_19 ) )
    {
      fc::optional< std::reference_wrapper< const comment_cashout_ex_object > > parent_comment_cashout_ex;
      if( parent )
        parent_comment_cashout_ex = *( _db.find_comment_cashout_ex( *parent ) );
      _db.create< comment_cashout_ex_object >( new_comment, parent_comment_cashout_ex, reward_weight );
    }

    if( parent )
    {
      const comment_cashout_object* parent_cashout = _db.find_comment_cashout( *parent );
      if( parent_cashout )
      {
        _db.modify( *parent_cashout, [&]( comment_cashout_object& p )
        {
          p.on_reply( _now );
        } );
      }
    }

  }
  else // start edit case
  {
    const auto& comment = *itr;

    if( _db.has_hardfork( HIVE_HARDFORK_0_21__3313 ) )
    {
      FC_ASSERT( _now - auth.last_post_edit >= HIVE_MIN_COMMENT_EDIT_INTERVAL, "Can only perform one comment edit per block." );
    }

    if( !_db.has_hardfork( HIVE_HARDFORK_0_17__772 ) )
    {
      const comment_cashout_object* comment_cashout = _db.find_comment_cashout( comment );
      FC_ASSERT( comment_cashout, "Comment cashout object must exist" );
      if( _db.has_hardfork( HIVE_HARDFORK_0_14__306 ) )
        FC_ASSERT( _db.calculate_discussion_payout_time( comment, *comment_cashout ) != fc::time_point_sec::maximum(), "The comment is archived." );
      else if( _db.has_hardfork( HIVE_HARDFORK_0_10 ) )
        FC_ASSERT( !_db.find_comment_cashout_ex( comment )->was_paid(), "Can only edit during the first 24 hours." );
    }

    if( !parent )
    {
      FC_ASSERT( comment.is_root(), "The parent of a comment cannot change." );
      //note for HiveMind: if someone tries to change 'category' (that no longer is part of consensus)
      //by providing different 'o.parent_permlink' than before, such change should be silently ignored
    }
    else if ( _db.has_hardfork( HIVE_HARDFORK_0_21__2203 ) )
    {
      //ABW: see creation tx 11ad62ee8f8e892cd5bd75fc2d3098427f7e47ac and edit tx dca209592c7129be36b069d033dfdb0f1f143b4e
      //both happened prior to HF21 when check was slightly more relaxed
      auto& parent_comment = _db.get_comment( o.parent_author, o.parent_permlink );
      FC_ASSERT( comment.get_parent_id() == parent_comment.get_id(), "The parent of a comment cannot change." );
    }

    _db.modify( auth, [&]( account_object& a )
    {
      a.last_post_edit = _now;
    });

  } // end EDIT case

} FC_CAPTURE_AND_RETHROW( (o) ) }

void escrow_transfer_evaluator::do_apply( const escrow_transfer_operation& o )
{
  try
  {
    const auto& from_account = _db.get_account(o.from);
    _db.get_account(o.to);
    _db.get_account(o.agent);

    FC_ASSERT( o.ratification_deadline > _db.head_block_time(), "The escrow ratification deadline must be after head block time." );
    FC_ASSERT( o.escrow_expiration > _db.head_block_time(), "The escrow expiration must be after head block time." );
    FC_ASSERT( from_account.pending_escrow_transfers < HIVE_MAX_PENDING_TRANSFERS, "Account already has the maximum number of open escrow transfers." );

    asset hive_spent = o.hive_amount;
    asset hbd_spent = o.hbd_amount;
    if( o.fee.symbol == HIVE_SYMBOL )
      hive_spent += o.fee;
    else
      hbd_spent += o.fee;

    _db.adjust_balance( from_account, -hive_spent );
    _db.adjust_balance( from_account, -hbd_spent );

    _db.create<escrow_object>( o.from, o.to, o.agent, o.hive_amount, o.hbd_amount, o.fee, o.ratification_deadline, o.escrow_expiration, o.escrow_id );
    _db.modify( from_account, []( account_object& a )
    {
      a.pending_escrow_transfers++;
    } );
  }
  FC_CAPTURE_AND_RETHROW( (o) )
}

void escrow_approve_evaluator::do_apply( const escrow_approve_operation& o )
{
  try
  {

    const auto& escrow = _db.get_escrow( o.from, o.escrow_id );

    FC_ASSERT( escrow.to == o.to, "Operation 'to' (${o}) does not match escrow 'to' (${e}).", ("o", o.to)("e", escrow.to) );
    FC_ASSERT( escrow.agent == o.agent, "Operation 'agent' (${a}) does not match escrow 'agent' (${e}).", ("o", o.agent)("e", escrow.agent) );
    FC_ASSERT( escrow.ratification_deadline >= _db.head_block_time(), "The escrow ratification deadline has passed. Escrow can no longer be ratified." );

    bool reject_escrow = !o.approve;

    if( o.who == o.to )
    {
      FC_ASSERT( !escrow.to_approved, "Account 'to' (${t}) has already approved the escrow.", ("t", o.to) );

      if( !reject_escrow )
      {
        _db.modify( escrow, [&]( escrow_object& esc )
        {
          esc.to_approved = true;
        });
      }
    }
    if( o.who == o.agent )
    {
      FC_ASSERT( !escrow.agent_approved, "Account 'agent' (${a}) has already approved the escrow.", ("a", o.agent) );

      if( !reject_escrow )
      {
        _db.modify( escrow, [&]( escrow_object& esc )
        {
          esc.agent_approved = true;
        });
      }
    }

    if( reject_escrow )
    {
      _db.adjust_balance( o.from, escrow.get_hive_balance() );
      _db.adjust_balance( o.from, escrow.get_hbd_balance() );
      _db.adjust_balance( o.from, escrow.get_fee() );

      _db.push_virtual_operation( escrow_rejected_operation( o.from, o.to, o.agent, o.escrow_id,
        escrow.get_hbd_balance(), escrow.get_hive_balance(), escrow.get_fee() ) );

      _db.modify( _db.get_account( escrow.from ), []( account_object& a )
      {
        a.pending_escrow_transfers--;
      } );
      _db.remove( escrow );
    }
    else if( escrow.to_approved && escrow.agent_approved )
    {
      _db.adjust_balance( o.agent, escrow.get_fee() );

      _db.push_virtual_operation( escrow_approved_operation( o.from, o.to, o.agent,
        o.escrow_id, escrow.get_fee() ) );

      _db.modify( escrow, [&]( escrow_object& esc )
      {
        esc.pending_fee.amount = 0;
      });
    }
  }
  FC_CAPTURE_AND_RETHROW( (o) )
}

void escrow_dispute_evaluator::do_apply( const escrow_dispute_operation& o )
{
  try
  {
    _db.get_account( o.from ); // Verify from account exists

    const auto& e = _db.get_escrow( o.from, o.escrow_id );
    FC_ASSERT( _db.head_block_time() < e.escrow_expiration, "Disputing the escrow must happen before expiration." );
    FC_ASSERT( e.to_approved && e.agent_approved, "The escrow must be approved by all parties before a dispute can be raised." );
    FC_ASSERT( !e.disputed, "The escrow is already under dispute." );
    FC_ASSERT( e.to == o.to, "Operation 'to' (${o}) does not match escrow 'to' (${e}).", ("o", o.to)("e", e.to) );
    FC_ASSERT( e.agent == o.agent, "Operation 'agent' (${a}) does not match escrow 'agent' (${e}).", ("o", o.agent)("e", e.agent) );

    _db.modify( e, [&]( escrow_object& esc )
    {
      esc.disputed = true;
    });
  }
  FC_CAPTURE_AND_RETHROW( (o) )
}

void escrow_release_evaluator::do_apply( const escrow_release_operation& o )
{
  try
  {
    const auto& from_account = _db.get_account( o.from );

    const auto& e = _db.get_escrow( o.from, o.escrow_id );
    FC_ASSERT( e.get_hive_balance() >= o.hive_amount, "Release amount exceeds escrow balance. Amount: ${a}, Balance: ${b}", ("a", o.hive_amount)("b", e.get_hive_balance()) );
    FC_ASSERT( e.get_hbd_balance() >= o.hbd_amount, "Release amount exceeds escrow balance. Amount: ${a}, Balance: ${b}", ("a", o.hbd_amount)("b", e.get_hbd_balance()) );
    FC_ASSERT( e.to == o.to, "Operation 'to' (${o}) does not match escrow 'to' (${e}).", ("o", o.to)("e", e.to) );
    FC_ASSERT( e.agent == o.agent, "Operation 'agent' (${a}) does not match escrow 'agent' (${e}).", ("o", o.agent)("e", e.agent) );
    FC_ASSERT( o.receiver == e.from || o.receiver == e.to, "Funds must be released to 'from' (${f}) or 'to' (${t})", ("f", e.from)("t", e.to) );
    FC_ASSERT( e.to_approved && e.agent_approved, "Funds cannot be released prior to escrow approval." );

    // If there is a dispute regardless of expiration, the agent can release funds to either party
    if( e.disputed )
    {
      FC_ASSERT( o.who == e.agent, "Only 'agent' (${a}) can release funds in a disputed escrow.", ("a", e.agent) );
    }
    else
    {
      FC_ASSERT( o.who == e.from || o.who == e.to, "Only 'from' (${f}) and 'to' (${t}) can release funds from a non-disputed escrow", ("f", e.from)("t", e.to) );

      if( e.escrow_expiration > _db.head_block_time() )
      {
        // If there is no dispute and escrow has not expired, either party can release funds to the other.
        if( o.who == e.from )
        {
          FC_ASSERT( o.receiver == e.to, "Only 'from' (${f}) can release funds to 'to' (${t}).", ("f", e.from)("t", e.to) );
        }
        else if( o.who == e.to )
        {
          FC_ASSERT( o.receiver == e.from, "Only 'to' (${t}) can release funds to 'from' (${t}).", ("f", e.from)("t", e.to) );
        }
      }
    }
    // If escrow expires and there is no dispute, either party can release funds to either party.

    _db.adjust_balance( o.receiver, o.hive_amount );
    _db.adjust_balance( o.receiver, o.hbd_amount );

    _db.modify( e, [&]( escrow_object& esc )
    {
      esc.hive_balance -= o.hive_amount;
      esc.hbd_balance -= o.hbd_amount;
    });

    if( e.get_hive_balance().amount == 0 && e.get_hbd_balance().amount == 0 )
    {
      _db.modify( from_account, []( account_object& a )
      {
        a.pending_escrow_transfers--;
      } );
      _db.remove( e );
    }
  }
  FC_CAPTURE_AND_RETHROW( (o) )
}

void transfer_evaluator::do_apply( const transfer_operation& o )
{
  if ( _db.has_hardfork(HIVE_HARDFORK_1_24) && o.amount.symbol == HIVE_SYMBOL && _db.is_treasury( o.to ) ) {
    const auto &fhistory = _db.get_feed_history();

    FC_ASSERT(!fhistory.current_median_history.is_null(), "Cannot send HIVE to ${s} because there is no price feed.", ("s", o.to ));

    auto amount_to_transfer = o.amount * fhistory.current_median_history;

    _db.adjust_balance(o.from, -o.amount);
    _db.adjust_balance(o.to, amount_to_transfer);

    _db.adjust_supply(-o.amount);

    if (amount_to_transfer.amount > 0)
      _db.adjust_supply(amount_to_transfer);

    // o.to will always be the treasury so no need to call _db.get_treasury
    _db.push_virtual_operation( dhf_conversion_operation( o.to, o.amount, amount_to_transfer ) );
    return;
  } else if( _db.has_hardfork( HIVE_HARDFORK_0_21__3343 ) )
  {
    FC_ASSERT( o.amount.symbol == HBD_SYMBOL || !_db.is_treasury( o.to ), "Can only transfer HBD or HIVE to ${s}", ("s", o.to ) );
  }

  _db.adjust_balance( o.from, -o.amount );
  _db.adjust_balance( o.to, o.amount );
}

void transfer_to_vesting_evaluator::do_apply( const transfer_to_vesting_operation& o )
{
  const auto& from_account = _db.get_account(o.from);
  const auto& to_account = o.to.size() ? _db.get_account(o.to) : from_account;

  if( _db.has_hardfork( HIVE_HARDFORK_0_21__3343 ) )
  {
    FC_ASSERT( o.amount.symbol == HBD_SYMBOL || !_db.is_treasury( o.to ),
      "Can only transfer HBD to ${s}", ("s", o.to ) );
  }

  _db.adjust_balance( from_account, -o.amount );

  asset amount_vested;

  /*
    Voting immediately when `transfer_to_vesting` is executed is dangerous,
    because malicious accounts would take over whole HIVE network.
    Therefore an idea is based on voting deferring. Default value is 30 days.
    This range of time is enough long to defeat/block potential malicious intention.
  */
  if( _db.has_hardfork( HIVE_HARDFORK_1_24 ) )
  {
    amount_vested = _db.adjust_account_vesting_balance( to_account, o.amount, false/*to_reward_balance*/, []( asset vests_created ) {} );

    delayed_voting dv( _db );
    dv.add_delayed_value( to_account, _db.head_block_time(), amount_vested.amount.value );
  }
  else
  {
    amount_vested = _db.create_vesting( to_account, o.amount );   
  }

  /// Emit this vop unconditionally, since VESTS balance changed immediately, indepdenent to subsequent updates of account voting power done inside `delayed_voting` mechanism.
  _db.push_virtual_operation(transfer_to_vesting_completed_operation(from_account.get_name(), to_account.get_name(), o.amount, amount_vested));
}

void withdraw_vesting_evaluator::do_apply( const withdraw_vesting_operation& o )
{
  const auto& account = _db.get_account( o.account );

  if( o.vesting_shares.amount < 0 )
  {
    FC_ASSERT( !_db.has_hardfork( HIVE_HARDFORK_0_20 ), "Cannot withdraw negative VESTS. account: ${account}, vests:${vests}",
      ("account", o.account)("vests", o.vesting_shares) );
    return;
  }

  FC_ASSERT( account.get_vesting() >= asset( 0, VESTS_SYMBOL ), "Account does not have sufficient Hive Power for withdraw." );
  FC_ASSERT( static_cast<asset>(account.get_vesting()) - account.delegated_vesting_shares >= o.vesting_shares, "Account does not have sufficient Hive Power for withdraw." );

  if( o.vesting_shares.amount == 0 )
  {
    if( _db.has_hardfork( HIVE_HARDFORK_0_5__57 ) )
      FC_ASSERT( account.vesting_withdraw_rate.amount  != 0, "This operation would not change the vesting withdraw rate." );

    _db.modify( account, [&]( account_object& a ) {
      a.vesting_withdraw_rate = asset( 0, VESTS_SYMBOL );
      a.next_vesting_withdrawal = time_point_sec::maximum();
      a.to_withdraw.amount = 0;
      a.withdrawn.amount = 0;
    });
  }
  else
  {
    int vesting_withdraw_intervals = HIVE_VESTING_WITHDRAW_INTERVALS_PRE_HF_16;
    if( _db.has_hardfork( HIVE_HARDFORK_0_16__551 ) )
      vesting_withdraw_intervals = HIVE_VESTING_WITHDRAW_INTERVALS; /// 13 weeks = 1 quarter of a year

    _db.modify( account, [&]( account_object& a )
    {
      auto new_vesting_withdraw_rate = asset( o.vesting_shares.amount / vesting_withdraw_intervals, VESTS_SYMBOL );

      if( new_vesting_withdraw_rate.amount == 0 )
          new_vesting_withdraw_rate.amount = 1;

      if( _db.has_hardfork( HIVE_HARDFORK_0_21 ) && new_vesting_withdraw_rate.amount * vesting_withdraw_intervals < o.vesting_shares.amount )
      {
        new_vesting_withdraw_rate.amount += 1;
      }

      if( _db.has_hardfork( HIVE_HARDFORK_0_5__57 ) )
        FC_ASSERT( account.vesting_withdraw_rate  != new_vesting_withdraw_rate, "This operation would not change the vesting withdraw rate." );

      a.vesting_withdraw_rate = new_vesting_withdraw_rate;
      a.next_vesting_withdrawal = _db.head_block_time() + fc::seconds(HIVE_VESTING_WITHDRAW_INTERVAL_SECONDS);
      a.to_withdraw.amount = o.vesting_shares.amount;
      a.withdrawn.amount = 0;
    });
  }
}

void set_withdraw_vesting_route_evaluator::do_apply( const set_withdraw_vesting_route_operation& o )
{
  try
  {
  const auto& from_account = _db.get_account( o.from_account );
  const auto& to_account = _db.get_account( o.to_account );
  const auto& wd_idx = _db.get_index< withdraw_vesting_route_index >().indices().get< by_withdraw_route >();
  auto itr = wd_idx.find( boost::make_tuple( from_account.get_name(), to_account.get_name() ) );

  if( _db.has_hardfork( HIVE_HARDFORK_0_21__3343 ) )
  {
    FC_ASSERT( !_db.is_treasury( o.to_account ), "Cannot withdraw vesting to ${s}", ("s", o.to_account ) );
  }

  if( itr == wd_idx.end() )
  {
    FC_ASSERT( o.percent != 0, "Cannot create a 0% destination." );
    FC_ASSERT( from_account.withdraw_routes < HIVE_MAX_WITHDRAW_ROUTES, "Account already has the maximum number of routes." );

    _db.create< withdraw_vesting_route_object >( [&]( withdraw_vesting_route_object& wvdo )
    {
      wvdo.from_account = from_account.get_name();
      wvdo.to_account = to_account.get_name();
      wvdo.percent = o.percent;
      wvdo.auto_vest = o.auto_vest;
    });

    _db.modify( from_account, [&]( account_object& a )
    {
      a.withdraw_routes++;
    });
  }
  else if( o.percent == 0 )
  {
    _db.remove( *itr );

    _db.modify( from_account, [&]( account_object& a )
    {
      a.withdraw_routes--;
    });
  }
  else
  {
    _db.modify( *itr, [&]( withdraw_vesting_route_object& wvdo )
    {
      wvdo.from_account = from_account.get_name();
      wvdo.to_account = to_account.get_name();
      wvdo.percent = o.percent;
      wvdo.auto_vest = o.auto_vest;
    });
  }

  itr = wd_idx.upper_bound( boost::make_tuple( from_account.get_name(), account_name_type() ) );
  uint16_t total_percent = 0;

  while( itr != wd_idx.end() && itr->from_account == from_account.get_name() )
  {
    total_percent += itr->percent;
    ++itr;
  }

  FC_ASSERT( total_percent <= HIVE_100_PERCENT, "More than 100% of vesting withdrawals allocated to destinations." );
  }
  FC_CAPTURE_AND_RETHROW()
}

void account_witness_proxy_evaluator::do_apply( const account_witness_proxy_operation& o )
{
  const auto& account = _db.get_account( o.account );
  FC_ASSERT( account.can_vote, "Account has declined the ability to vote and cannot proxy votes." );
  _db.modify( account, [&]( account_object& a) { a.update_governance_vote_expiration_ts(_db.head_block_time()); });

  _db.nullify_proxied_witness_votes( account );

  if( !o.is_clearing_proxy() ) {
    const auto& new_proxy = _db.get_account( o.proxy );
    FC_ASSERT( account.get_proxy() != new_proxy.get_id(), "Proxy must change." );
    flat_set<account_id_type> proxy_chain( { account.get_id(), new_proxy.get_id() } );
    proxy_chain.reserve( HIVE_MAX_PROXY_RECURSION_DEPTH + 1 );

    /// check for proxy loops and fail to update the proxy if it would create a loop
    auto cprox = &new_proxy;
    while( cprox->has_proxy() )
    {
      const auto& next_proxy = _db.get_account( cprox->get_proxy() );
      FC_ASSERT( proxy_chain.insert( next_proxy.get_id() ).second, "This proxy would create a proxy loop." );
      cprox = &next_proxy;
      FC_ASSERT( proxy_chain.size() <= HIVE_MAX_PROXY_RECURSION_DEPTH, "Proxy chain is too long." );
    }

    /// clear all individual vote records
    _db.clear_witness_votes( account );

    _db.modify( account, [&]( account_object& a ) {
      if( account.has_proxy() )
      {
        _db.push_virtual_operation( proxy_cleared_operation( account.get_name(), _db.get_account( account.get_proxy() ).get_name()) );
      }

      a.set_proxy( new_proxy );
    });

    /// add all new votes
    std::array<share_type, HIVE_MAX_PROXY_RECURSION_DEPTH + 1> delta;
    delta[0] = account.get_direct_governance_vote_power();
    for( int i = 0; i < HIVE_MAX_PROXY_RECURSION_DEPTH; ++i )
      delta[i+1] = account.proxied_vsf_votes[i];
    _db.adjust_proxied_witness_votes( account, delta );
  } else { /// we are clearing the proxy which means we simply update the account
    FC_ASSERT( account.has_proxy(), "Proxy must change." );

    _db.push_virtual_operation( proxy_cleared_operation( account.get_name(), _db.get_account( account.get_proxy() ).get_name()) );

    _db.modify( account, [&]( account_object& a ) {
      a.clear_proxy();
    });
  }
}


void account_witness_vote_evaluator::do_apply( const account_witness_vote_operation& o )
{
  const auto& voter = _db.get_account( o.account );
  FC_ASSERT( !voter.has_proxy(), "A proxy is currently set, please clear the proxy before voting for a witness." );
  FC_ASSERT( voter.can_vote, "Account has declined its voting rights." );
  _db.modify( voter, [&]( account_object& a) { a.update_governance_vote_expiration_ts(_db.head_block_time()); });

  const auto& witness = _db.get_witness( o.witness );

  const auto& by_account_witness_idx = _db.get_index< witness_vote_index >().indices().get< by_account_witness >();
  auto itr = by_account_witness_idx.find( boost::make_tuple( voter.get_name(), witness.owner ) );

  if( itr == by_account_witness_idx.end() ) {
    FC_ASSERT( o.approve, "Vote doesn't exist, user must indicate a desire to approve witness." );

    if ( _db.has_hardfork( HIVE_HARDFORK_0_2 ) )
    {
      FC_ASSERT( voter.witnesses_voted_for < HIVE_MAX_ACCOUNT_WITNESS_VOTES, "Account has voted for too many witnesses." ); // TODO: Remove after hardfork 2

      _db.create<witness_vote_object>( [&]( witness_vote_object& v ) {
          v.witness = witness.owner;
          v.account = voter.get_name();
      });

      if( _db.has_hardfork( HIVE_HARDFORK_0_3 ) ) {
        _db.adjust_witness_vote( witness, voter.get_governance_vote_power() );
      }
      else {
        _db.adjust_proxied_witness_votes( voter, voter.get_governance_vote_power() );
      }

    } else {

      _db.create<witness_vote_object>( [&]( witness_vote_object& v ) {
          v.witness = witness.owner;
          v.account = voter.get_name();
      });
      _db.modify( witness, [&]( witness_object& w ) {
          w.votes += voter.get_governance_vote_power();
      });

    }
    _db.modify( voter, [&]( account_object& a ) {
      a.witnesses_voted_for++;
    });

  } else {
    FC_ASSERT( !o.approve, "Vote currently exists, user must indicate a desire to reject witness." );

    if (  _db.has_hardfork( HIVE_HARDFORK_0_2 ) ) {
      if( _db.has_hardfork( HIVE_HARDFORK_0_3 ) )
        _db.adjust_witness_vote( witness, -voter.get_governance_vote_power() );
      else
        _db.adjust_proxied_witness_votes( voter, -voter.get_governance_vote_power() );
    } else  {
      _db.modify( witness, [&]( witness_object& w ) {
        w.votes -= voter.get_governance_vote_power();
      });
    }
    _db.modify( voter, [&]( account_object& a ) {
      a.witnesses_voted_for--;
    });
    _db.remove( *itr );
  }
}

void pre_hf20_vote_evaluator( const vote_operation& o, database& _db )
{
  const auto& comment = _db.get_comment( o.author, o.permlink );
  const comment_cashout_object* comment_cashout = _db.find_comment_cashout( comment );

  const auto& voter = _db.get_account( o.voter );

  FC_ASSERT( voter.can_vote, "Voter has declined their voting rights." );

  if( comment_cashout )
  {
    if( o.weight > 0 ) FC_ASSERT( comment_cashout->allows_votes(), "Votes are not allowed on the comment." );
  }

  if( !comment_cashout || ( _db.has_hardfork( HIVE_HARDFORK_0_12__177 ) &&
    _db.calculate_discussion_payout_time( comment, *comment_cashout ) == fc::time_point_sec::maximum() ) )
  {
    return; // comment already paid
  }

  auto _now = _db.head_block_time();

  int64_t current_power = 0;
  {
    int64_t elapsed_seconds = _now.sec_since_epoch() - voter.voting_manabar.last_update_time;
    if( _db.has_hardfork( HIVE_HARDFORK_0_11 ) )
      FC_ASSERT( elapsed_seconds >= HIVE_MIN_VOTE_INTERVAL_SEC, "Can only vote once every 3 seconds." );
    int64_t regenerated_power = (HIVE_100_PERCENT * elapsed_seconds) / HIVE_VOTING_MANA_REGENERATION_SECONDS;
    current_power = std::min( int64_t(voter.voting_manabar.current_mana) + regenerated_power, int64_t(HIVE_100_PERCENT) );
    FC_ASSERT( current_power > 0, "Account currently does not have voting power." );
  }
  int64_t abs_weight = abs(o.weight);
  // Less rounding error would occur if we did the division last, but we need to maintain backward
  // compatibility with the previous implementation which was replaced in #1285
  int64_t used_power = ((current_power * abs_weight) / HIVE_100_PERCENT) * (60*60*24);

  const dynamic_global_property_object& dgpo = _db.get_dynamic_global_properties();

  // The second multiplication is rounded up as of HF14#259
  int64_t max_vote_denom = dgpo.vote_power_reserve_rate * HIVE_VOTING_MANA_REGENERATION_SECONDS;
  FC_ASSERT( max_vote_denom > 0 );

  if( !_db.has_hardfork( HIVE_HARDFORK_0_14__259 ) )
  {
    used_power = (used_power / max_vote_denom)+1;
  }
  else
  {
    used_power = (used_power + max_vote_denom - 1) / max_vote_denom;
  }
  FC_ASSERT( used_power <= current_power, "Account does not have enough power to vote." );

  int64_t abs_rshares = fc::uint128_to_uint64((uint128_t( _db.get_effective_vesting_shares( voter, VESTS_SYMBOL ).amount.value ) * used_power) / (HIVE_100_PERCENT));
  if( !_db.has_hardfork( HIVE_HARDFORK_0_14__259 ) && abs_rshares == 0 ) abs_rshares = 1;

  if( _db.has_hardfork( HIVE_HARDFORK_0_14__259 ) )
  {
    FC_ASSERT( abs_rshares > HIVE_VOTE_DUST_THRESHOLD || o.weight == 0, "Voting weight is too small, please accumulate more voting power or Hive Power." );
  }
  else if( _db.has_hardfork( HIVE_HARDFORK_0_13__248 ) )
  {
    FC_ASSERT( abs_rshares > HIVE_VOTE_DUST_THRESHOLD || abs_rshares == 1, "Voting weight is too small, please accumulate more voting power or Hive Power." );
  }

  const auto& comment_vote_idx = _db.get_index< comment_vote_index, by_comment_voter >();
  auto itr = comment_vote_idx.find( boost::make_tuple( comment.get_id(), voter.get_id() ) );

  /// this is the rshares voting for or against the post
  int64_t rshares = o.weight < 0 ? -abs_rshares : abs_rshares;
  {
    auto previous_vote_rshares = ( itr == comment_vote_idx.end() ? 0 : itr->get_rshares() );
    if( rshares > previous_vote_rshares )
    {
      if( _db.has_hardfork( HIVE_HARDFORK_0_17__900 ) )
        FC_ASSERT( _now < comment_cashout->get_cashout_time() - HIVE_UPVOTE_LOCKOUT_HF17, "Cannot increase payout within last twelve hours before payout." );
      else if( _db.has_hardfork( HIVE_HARDFORK_0_7 ) )
        FC_ASSERT( _now < _db.calculate_discussion_payout_time( comment, *comment_cashout ) - HIVE_UPVOTE_LOCKOUT_HF7, "Cannot increase payout within last minute before payout." );
    }
  }

  _db.modify( voter, [&]( account_object& a )
  {
    a.voting_manabar.current_mana = current_power - used_power; // always nonnegative
    a.last_vote_time = _now;
    a.voting_manabar.last_update_time = _now.sec_since_epoch();
  } );

  /// if the current net_rshares is less than 0, the post is getting 0 rewards so it is not factored into total rshares^2
  fc::uint128_t old_rshares = std::max( comment_cashout->get_net_rshares(), int64_t( 0 ) );
  const auto* comment_cashout_ex = _db.find_comment_cashout_ex( comment );

  if( !_db.has_hardfork( HIVE_HARDFORK_0_17__769 ) )
  {
    const auto& root = _db.get( comment_cashout_ex->get_root_id() );
    const comment_cashout_object* root_cashout = _db.find_comment_cashout( root );
    const comment_cashout_ex_object* root_cashout_ex = _db.find_comment_cashout_ex( root );
    FC_ASSERT( root_cashout && root_cashout_ex );

    _db.modify( *root_cashout, [&]( comment_cashout_object& c )
    {
      auto old_root_abs_rshares = root_cashout_ex->get_children_abs_rshares();
      _db.modify( *root_cashout_ex, [&]( comment_cashout_ex_object& c_ex )
      {
        c_ex.accumulate_children_abs_rshares( abs_rshares );
      } );

      if( _db.has_hardfork( HIVE_HARDFORK_0_12__177 ) && root_cashout_ex->was_paid() )
      {
        c.set_cashout_time( root_cashout_ex->get_last_payout() + HIVE_SECOND_CASHOUT_WINDOW );
      }
      else
      {
        fc::uint128_t cur_cashout_time_sec = _db.calculate_discussion_payout_time( comment, *comment_cashout ).sec_since_epoch();
        fc::uint128_t avg_cashout_sec = 0;
        if( _db.has_hardfork( HIVE_HARDFORK_0_14__259 ) && abs_rshares == 0 )
        {
          avg_cashout_sec = cur_cashout_time_sec;
        }
        else
        {
          fc::uint128_t new_cashout_time_sec = 0;
          if( _db.has_hardfork( HIVE_HARDFORK_0_12__177 ) && !_db.has_hardfork( HIVE_HARDFORK_0_13__257 ) )
            new_cashout_time_sec = _now.sec_since_epoch() + HIVE_CASHOUT_WINDOW_SECONDS_PRE_HF17;
          else
            new_cashout_time_sec = _now.sec_since_epoch() + HIVE_CASHOUT_WINDOW_SECONDS_PRE_HF12;
          avg_cashout_sec = ( cur_cashout_time_sec * old_root_abs_rshares + new_cashout_time_sec * abs_rshares ) / root_cashout_ex->get_children_abs_rshares();
        }
        c.set_cashout_time( fc::time_point_sec( std::min( uint32_t( fc::uint128_to_uint64(avg_cashout_sec) ), root_cashout_ex->get_max_cashout_time().sec_since_epoch() ) ) );
      }

      if( root_cashout_ex->get_max_cashout_time() == fc::time_point_sec::maximum() )
      {
        _db.modify( *root_cashout_ex, [&]( comment_cashout_ex_object& c_ex )
        {
          c_ex.set_max_cashout_time( _now + fc::seconds( HIVE_MAX_CASHOUT_WINDOW_SECONDS ) );
        } );
      }
    } );
  }

  uint64_t vote_weight = 0;

  if( itr == comment_vote_idx.end() ) // new vote
  {
    FC_ASSERT( o.weight != 0, "Vote weight cannot be 0." );
    FC_ASSERT( abs_rshares > 0, "Cannot vote with 0 rshares." );

    auto old_vote_rshares = comment_cashout->get_vote_rshares();

    _db.modify( *comment_cashout, [&]( comment_cashout_object& c )
    {
      c.on_vote( o.weight );
      c.accumulate_vote_rshares( rshares, rshares > 0 ? rshares : 0 );
    });
    if( comment_cashout_ex )
    {
      _db.modify( *comment_cashout_ex, [&]( comment_cashout_ex_object& c_ex )
      {
        c_ex.accumulate_abs_rshares( abs_rshares );
      } );
    }

    /** this verifies uniqueness of voter
      *
      *  cv.weight / c.total_vote_weight ==> % of rshares increase that is accounted for by the vote
      *
      *  W(R) = B * R / ( R + 2S )
      *  W(R) is bounded above by B. B is fixed at 2^64 - 1, so all weights fit in a 64 bit integer.
      *
      *  The equation for an individual vote is:
      *    W(R_N) - W(R_N-1), which is the delta increase of proportional weight
      *
      *  c.total_vote_weight =
      *    W(R_1) - W(R_0) +
      *    W(R_2) - W(R_1) + ...
      *    W(R_N) - W(R_N-1) = W(R_N) - W(R_0)
      *
      *  Since W(R_0) = 0, c.total_vote_weight is also bounded above by B and will always fit in a 64 bit integer.
      *
    **/
    uint64_t max_vote_weight = 0;
    {
      bool curation_reward_eligible = rshares > 0 && comment_cashout->allows_curation_rewards();
      if( curation_reward_eligible && comment_cashout_ex )
        curation_reward_eligible = !comment_cashout_ex->was_paid();
      if( curation_reward_eligible && _db.has_hardfork( HIVE_HARDFORK_0_17__774 ) )
        curation_reward_eligible = _db.get_curation_rewards_percent() > 0;

      if( curation_reward_eligible )
      {
        if( comment_cashout->get_creation_time() < fc::time_point_sec(HIVE_HARDFORK_0_6_REVERSE_AUCTION_TIME) )
        {
          u512 rshares3(rshares);
          u256 total2( comment_cashout_ex->get_abs_rshares() );

          if( !_db.has_hardfork( HIVE_HARDFORK_0_1 ) )
          {
            rshares3 *= 1000000;
            total2 *= 1000000;
          }

          rshares3 = rshares3 * rshares3 * rshares3;

          total2 *= total2;
          vote_weight = static_cast<uint64_t>( rshares3 / total2 );
        } else {// cv.weight = W(R_1) - W(R_0)
          const uint128_t two_s = 2 * util::get_content_constant_s();
          if( _db.has_hardfork( HIVE_HARDFORK_0_17__774 ) )
          {
            const auto& reward_fund = _db.get_reward_fund();
            auto curve = !_db.has_hardfork( HIVE_HARDFORK_0_19__1052 ) && comment_cashout->get_creation_time() > HIVE_HF_19_SQRT_PRE_CALC
                      ? curve_id::square_root : reward_fund.curation_reward_curve;
            uint64_t old_weight = fc::uint128_to_uint64(util::evaluate_reward_curve( old_vote_rshares, curve, reward_fund.content_constant ));
            uint64_t new_weight = fc::uint128_to_uint64(util::evaluate_reward_curve( comment_cashout->get_vote_rshares(), curve, reward_fund.content_constant ));
            vote_weight = new_weight - old_weight;
          }
          else if ( _db.has_hardfork( HIVE_HARDFORK_0_1 ) )
          {
            uint64_t old_weight = fc::uint128_to_uint64( ( std::numeric_limits< uint64_t >::max() * fc::uint128_t( old_vote_rshares ) ) / ( two_s + old_vote_rshares ) );
            uint64_t new_weight = fc::uint128_to_uint64( ( std::numeric_limits< uint64_t >::max() * fc::uint128_t( comment_cashout->get_vote_rshares() ) ) / ( two_s + comment_cashout->get_vote_rshares() ) );
            vote_weight = new_weight - old_weight;
          }
          else
          {
            uint64_t old_weight = fc::uint128_to_uint64( ( std::numeric_limits< uint64_t >::max() * fc::uint128_t( 1000000 * old_vote_rshares ) ) / ( two_s + ( 1000000 * old_vote_rshares ) ) );
            uint64_t new_weight = fc::uint128_to_uint64( ( std::numeric_limits< uint64_t >::max() * fc::uint128_t( 1000000 * comment_cashout->get_vote_rshares() ) ) / ( two_s + ( 1000000 * comment_cashout->get_vote_rshares() ) ) );
            vote_weight = new_weight - old_weight;
          }
        }

        max_vote_weight = vote_weight;

        if( _now > fc::time_point_sec(HIVE_HARDFORK_0_6_REVERSE_AUCTION_TIME) )  /// start enforcing this prior to the hardfork
        {
          /// discount weight by time
          uint128_t w(max_vote_weight);
          uint64_t delta_t = std::min( uint64_t((_now - comment_cashout->get_creation_time()).to_seconds()), dgpo.reverse_auction_seconds );

          w *= delta_t;
          w /= dgpo.reverse_auction_seconds;
          vote_weight = fc::uint128_to_uint64(w);
        }
      }
    }

    _db.create<comment_vote_object>( voter, comment, _now, o.weight, vote_weight, rshares );

    if( max_vote_weight ) // Optimization
    {
      _db.modify( *comment_cashout, [&]( comment_cashout_object& c )
      {
        c.accumulate_vote_weight( max_vote_weight );
      });
    }
  }
  else // edit of existing vote
  {
    FC_ASSERT( itr->get_number_of_changes() < HIVE_MAX_VOTE_CHANGES, "Voter has used the maximum number of vote changes on this comment." );

    if( _db.has_hardfork( HIVE_HARDFORK_0_6__112 ) )
      FC_ASSERT( itr->get_vote_percent() != o.weight, "You have already voted in a similar way." );

    _db.modify( *comment_cashout, [&]( comment_cashout_object& c )
    {
      c.on_vote( o.weight, itr->get_vote_percent() );
      c.accumulate_vote_rshares( rshares - itr->get_rshares(), 0 );
      c.accumulate_vote_weight( -itr->get_weight() );
    } );
    if( comment_cashout_ex )
    {
      _db.modify( *comment_cashout_ex, [&]( comment_cashout_ex_object& c_ex )
      {
        c_ex.accumulate_abs_rshares( abs_rshares );
      } );
    }

    _db.modify( *itr, [&]( comment_vote_object& cv )
    {
      cv.set( _now, o.weight, 0, rshares );
    } );
  }

  if( !_db.has_hardfork( HIVE_HARDFORK_0_17__774 ) )
  {
    fc::uint128_t new_rshares = std::max( comment_cashout->get_net_rshares(), int64_t( 0 ) );
    new_rshares = util::evaluate_reward_curve( new_rshares );
    old_rshares = util::evaluate_reward_curve( old_rshares );
    _db.adjust_rshares2( old_rshares, new_rshares );
  }

  _db.push_virtual_operation( effective_comment_vote_operation( o.voter, o.author, o.permlink, vote_weight, rshares, comment_cashout->get_total_vote_weight() ) );
}

void hf20_vote_evaluator( const vote_operation& o, database& _db )
{
  const auto& comment = _db.get_comment( o.author, o.permlink );
  const comment_cashout_object* comment_cashout = _db.find_comment_cashout( comment );

  const auto& voter   = _db.get_account( o.voter );
  const auto& dgpo    = _db.get_dynamic_global_properties();

  FC_ASSERT( voter.can_vote, "Voter has declined their voting rights." );

  if( comment_cashout )
  {
    if( o.weight > 0 ) FC_ASSERT( comment_cashout->allows_votes(), "Votes are not allowed on the comment." );
  }

  if( !comment_cashout || _db.calculate_discussion_payout_time( comment, *comment_cashout ) == fc::time_point_sec::maximum() )
  {
    return; // comment already paid
  }

  auto _now = _db.head_block_time();
  FC_ASSERT( _now < comment_cashout->get_cashout_time(), "Comment is actively being rewarded. Cannot vote on comment." );
  if( !_db.has_hardfork( HIVE_HARDFORK_1_26_NO_VOTE_COOLDOWN ) )
    FC_ASSERT( ( _now - voter.last_vote_time ).to_seconds() >= HIVE_MIN_VOTE_INTERVAL_SEC, "Can only vote once every 3 seconds." );

  const auto& comment_vote_idx = _db.get_index< comment_vote_index, by_comment_voter >();
  auto itr = comment_vote_idx.find( boost::make_tuple( comment.get_id(), voter.get_id() ) );

  int16_t previous_vote_percent = 0;
  int64_t previous_rshares = 0;
  int64_t previous_positive_rshares = 0;
  uint64_t previous_vote_weight = 0;
  if( itr == comment_vote_idx.end() )
  {
    FC_ASSERT( o.weight != 0, "Vote weight cannot be 0." );
  }
  else
  {
    FC_ASSERT( itr->get_number_of_changes() < HIVE_MAX_VOTE_CHANGES, "Voter has used the maximum number of vote changes on this comment." );
    FC_ASSERT( itr->get_vote_percent() != o.weight, "Your current vote on this comment is identical to this vote." );
    previous_vote_percent = itr->get_vote_percent();
    previous_rshares = itr->get_rshares();
    if( previous_rshares > 0 )
      previous_positive_rshares = previous_rshares;
    previous_vote_weight = itr->get_weight();
  }

  _db.modify( voter, [&]( account_object& a )
  {
    util::update_manabar( dgpo, a );
  });

  int16_t abs_weight = abs( o.weight );
  uint128_t used_mana = 0;

  FC_TODO( "This hardfork check should not be needed. Remove after HF21 if that is the case." );
  if( _db.has_hardfork( HIVE_HARDFORK_0_21__3336 ) && dgpo.downvote_pool_percent && o.weight < 0 )
  {
    if( _db.has_hardfork( HIVE_HARDFORK_0_22__3485 ) )
    {
      used_mana = ( std::max( ( ( uint128_t( voter.downvote_manabar.current_mana ) * HIVE_100_PERCENT ) / dgpo.downvote_pool_percent ),
                      uint128_t( voter.voting_manabar.current_mana ) )
            * abs_weight * 60 * 60 * 24 ) / HIVE_100_PERCENT;
    }
    else
    {
      used_mana = ( std::max( ( uint128_t( voter.downvote_manabar.current_mana * HIVE_100_PERCENT ) / dgpo.downvote_pool_percent ),
                      uint128_t( voter.voting_manabar.current_mana ) )
            * abs_weight * 60 * 60 * 24 ) / HIVE_100_PERCENT;
    }
  }
  else
  {
    used_mana = ( uint128_t( voter.voting_manabar.current_mana ) * abs_weight * 60 * 60 * 24 ) / HIVE_100_PERCENT;
  }

  int64_t max_vote_denom = dgpo.vote_power_reserve_rate * HIVE_VOTING_MANA_REGENERATION_SECONDS;
  FC_ASSERT( max_vote_denom > 0 );

  used_mana = ( used_mana + max_vote_denom - 1 ) / max_vote_denom;
  if( _db.has_hardfork( HIVE_HARDFORK_0_21__3336 ) && dgpo.downvote_pool_percent && o.weight < 0 )
  {
    FC_ASSERT( voter.voting_manabar.current_mana + voter.downvote_manabar.current_mana > fc::uint128_to_int64(used_mana),
      "Account does not have enough mana to downvote. voting_mana: ${v} downvote_mana: ${d} required_mana: ${r}",
      ("v", voter.voting_manabar.current_mana)("d", voter.downvote_manabar.current_mana)("r", fc::uint128_to_int64(used_mana)) );
  }
  else
  {
    FC_ASSERT( voter.voting_manabar.has_mana( fc::uint128_to_int64(used_mana) ), "Account does not have enough mana to vote." );
  }

  int64_t abs_rshares = fc::uint128_to_int64(used_mana);

  abs_rshares -= HIVE_VOTE_DUST_THRESHOLD;
  abs_rshares = std::max( int64_t(0), abs_rshares );

  uint32_t cashout_delta = ( comment_cashout->get_cashout_time() - _now ).to_seconds();

  if( cashout_delta < HIVE_UPVOTE_LOCKOUT_SECONDS )
  {
    abs_rshares = (int64_t) fc::uint128_to_uint64( ( uint128_t( abs_rshares ) * cashout_delta ) / HIVE_UPVOTE_LOCKOUT_SECONDS );
  }

  _db.modify( voter, [&]( account_object& a )
  {
    if( _db.has_hardfork( HIVE_HARDFORK_0_21__3336 ) && dgpo.downvote_pool_percent > 0 && o.weight < 0 )
    {
      if( fc::uint128_to_int64(used_mana) > a.downvote_manabar.current_mana )
      {
        /* used mana is always less than downvote_mana + voting_mana because the amount used
          * is a fraction of max( downvote_mana, voting_mana ). If more mana is consumed than
          * there is downvote_mana, then it is because voting_mana is greater, and used_mana
          * is strictly smaller than voting_mana. This is the same reason why a check is not
          * required when using voting mana on its own as an upvote.
          */
        auto remainder = fc::uint128_to_int64(used_mana) - a.downvote_manabar.current_mana;
        a.downvote_manabar.use_mana( a.downvote_manabar.current_mana );
        a.voting_manabar.use_mana( remainder );
      }
      else
      {
        a.downvote_manabar.use_mana( fc::uint128_to_int64(used_mana) );
      }
    }
    else
    {
      a.voting_manabar.use_mana( fc::uint128_to_int64(used_mana) );
    }

    a.last_vote_time = _now;
  } );

  /// this is the rshares voting for or against the post
  int64_t rshares = o.weight < 0 ? -abs_rshares : abs_rshares;
  uint64_t vote_weight = 0;

  _db.modify( *comment_cashout, [&]( comment_cashout_object& c )
  {
    c.on_vote( o.weight, previous_vote_percent, abs_rshares != 0 || _db.has_hardfork( HIVE_HARDFORK_1_26_DUST_VOTE_FIX ) );

    auto old_vote_rshares = comment_cashout->get_vote_rshares();
    uint64_t max_vote_weight = 0;
    if( itr == comment_vote_idx.end() || _db.has_hardfork( HIVE_HARDFORK_1_26_NO_VOTE_EDIT_PENALTY ) )
    {
      c.accumulate_vote_rshares( rshares - previous_rshares, ( rshares > 0 ? rshares : 0 ) - previous_positive_rshares );
      if( _db.has_hardfork( HIVE_HARDFORK_1_26_NO_VOTE_EDIT_PENALTY ) )
        old_vote_rshares -= previous_positive_rshares;

      /** this verifies uniqueness of voter
        *
        *  cv.weight / c.total_vote_weight ==> % of rshares increase that is accounted for by the vote
        *
        *  W(R) = B * R / ( R + 2S )
        *  W(R) is bounded above by B. B is fixed at 2^64 - 1, so all weights fit in a 64 bit integer.
        *
        *  The equation for an individual vote is:
        *    W(R_N) - W(R_N-1), which is the delta increase of proportional weight
        *
        *  c.total_vote_weight =
        *    W(R_1) - W(R_0) +
        *    W(R_2) - W(R_1) + ...
        *    W(R_N) - W(R_N-1) = W(R_N) - W(R_0)
        *
        *  Since W(R_0) = 0, c.total_vote_weight is also bounded above by B and will always fit in a 64 bit integer.
        *
      **/
      {
        bool curation_reward_eligible = rshares > 0 && comment_cashout->allows_curation_rewards() &&
          ( _db.get_curation_rewards_percent() > 0 );

        if( curation_reward_eligible )
        {
          // cv.weight = W(R_1) - W(R_0)
          const auto& reward_fund = _db.get_reward_fund();
          auto curve = reward_fund.curation_reward_curve;
          uint64_t old_weight = fc::uint128_to_uint64(util::evaluate_reward_curve( old_vote_rshares, curve, reward_fund.content_constant ));
          uint64_t new_weight = fc::uint128_to_uint64(util::evaluate_reward_curve( c.get_vote_rshares(), curve, reward_fund.content_constant ));

          if( old_weight < new_weight ) // old_weight > new_weight should never happen, but == is ok
          {
            uint64_t _seconds = ( _now - c.get_creation_time() ).to_seconds();

            vote_weight = new_weight - old_weight;

            //In HF25 `dgpo.reverse_auction_seconds` is set to zero. It's replaced by `dgpo.early_voting_seconds` and `dgpo.mid_voting_seconds`.
            if( _seconds < dgpo.reverse_auction_seconds )
            {
              max_vote_weight = vote_weight;

              /// discount weight by time
              uint128_t w( max_vote_weight );
              uint64_t delta_t = std::min( _seconds, uint64_t( dgpo.reverse_auction_seconds ) );

              w *= delta_t;
              w /= dgpo.reverse_auction_seconds;
              vote_weight = fc::uint128_to_uint64(w);
            }
            else if( _seconds >= dgpo.early_voting_seconds && dgpo.early_voting_seconds )
            {
              //Following values are chosen empirically
              const uint32_t phase_1_factor = 2;
              const uint32_t phase_2_factor = 8;

              if( _seconds < ( dgpo.early_voting_seconds + dgpo.mid_voting_seconds ) )
                vote_weight /= phase_1_factor;
              else
                vote_weight /= phase_2_factor;

              max_vote_weight = vote_weight;
            }
            else
            {
              max_vote_weight = vote_weight;
            }
          }
        }
      }
    }
    else // pre-HF26 vote edit
    {
      c.accumulate_vote_rshares( rshares - previous_rshares, 0 );
    }

    c.accumulate_vote_weight( max_vote_weight - previous_vote_weight );
  } );

  if( itr == comment_vote_idx.end() ) // new vote
  {
    _db.create<comment_vote_object>( voter, comment, _now, o.weight, vote_weight, rshares );
  }
  else // edit of existing vote
  {
    _db.modify( *itr, [&]( comment_vote_object& cv )
    {
      cv.set( _now, o.weight, vote_weight, rshares );
    });
  }

  _db.push_virtual_operation( effective_comment_vote_operation( o.voter, o.author, o.permlink, vote_weight, rshares, comment_cashout->get_total_vote_weight() ) );
}

void vote_evaluator::do_apply( const vote_operation& o )
{ try {
  if( _db.has_hardfork( HIVE_HARDFORK_0_20__2539 ) )
  {
    hf20_vote_evaluator( o, _db );
  }
  else
  {
    pre_hf20_vote_evaluator( o, _db );
  }
} FC_CAPTURE_AND_RETHROW( (o) ) }

void custom_evaluator::do_apply( const custom_operation& o )
{
  FC_TODO( "Check when this soft-fork was added and change to appropriate hardfork" );
  if( _db.is_in_control() || _db.has_hardfork( HIVE_HARDFORK_1_26_SOLIDIFY_OLD_SOFTFORKS ) )
  {
    FC_ASSERT( o.data.size() <= HIVE_CUSTOM_OP_DATA_MAX_LENGTH,
      "Operation data must be less than ${bytes} bytes.", ("bytes", HIVE_CUSTOM_OP_DATA_MAX_LENGTH) );
  }

  if( _db.has_hardfork( HIVE_HARDFORK_0_20 ) )
  {
    FC_TODO( "Check if the following could become part of operation validation (unconditional)" );
    FC_ASSERT( o.required_auths.size() <= HIVE_MAX_AUTHORITY_MEMBERSHIP,
      "Authority membership exceeded. Max: ${max} Current: ${n}", ("max", HIVE_MAX_AUTHORITY_MEMBERSHIP)("n", o.required_auths.size()) );
  }
}

void custom_json_evaluator::do_apply( const custom_json_operation& o )
{
  using hive::protocol::details::truncation_controller;

  FC_TODO( "Check when this soft-fork was added and change to appropriate hardfork" );
  if( _db.is_in_control() || _db.has_hardfork( HIVE_HARDFORK_1_26_SOLIDIFY_OLD_SOFTFORKS ) )
  {
    FC_ASSERT( o.json.length() <= HIVE_CUSTOM_OP_DATA_MAX_LENGTH,
      "Operation JSON must be less than ${bytes} bytes.", ("bytes", HIVE_CUSTOM_OP_DATA_MAX_LENGTH) );
  }

  if( _db.has_hardfork( HIVE_HARDFORK_0_20 ) )
  {
    size_t num_auths = o.required_auths.size() + o.required_posting_auths.size();
    FC_TODO( "Check if the following could become part of operation validation (unconditional)" );
    FC_ASSERT( num_auths <= HIVE_MAX_AUTHORITY_MEMBERSHIP,
      "Authority membership exceeded. Max: ${max} Current: ${n}", ("max", HIVE_MAX_AUTHORITY_MEMBERSHIP)("n", num_auths) );
  }

  std::shared_ptr< custom_operation_interpreter > eval = _db.get_custom_json_evaluator( o.id );
  if( !eval )
    return;

  try
  {
    auto _old_verify_status = truncation_controller::is_verifying_enabled();
    BOOST_SCOPE_EXIT(&_old_verify_status) { truncation_controller::set_verify( _old_verify_status ); } BOOST_SCOPE_EXIT_END

    if( !_db.is_in_control() )
      truncation_controller::set_verify( false );

    eval->apply( o );
  }
  catch( const fc::exception& e )
  {
    if( _db.is_in_control() )
      throw;
    //note: it is up to evaluator to unconditionally (regardless of is_in_control, working even during
    //replay) undo changes made during custom operation in case of exception;
    //generic_custom_operation_interpreter::apply_operations provides such protection (see issue #256)
  }
  catch(...)
  {
    elog( "Unexpected exception applying custom json evaluator." );
  }
}


void custom_binary_evaluator::do_apply( const custom_binary_operation& o )
{
  FC_TODO( "Check when this soft-fork was added and change to appropriate hardfork" );
  if( _db.is_in_control() || _db.has_hardfork( HIVE_HARDFORK_1_26_SOLIDIFY_OLD_SOFTFORKS ) )
  {
    FC_ASSERT( false, "custom_binary_operation is deprecated" );
    FC_ASSERT( o.data.size() <= HIVE_CUSTOM_OP_DATA_MAX_LENGTH,
      "Operation data must be less than ${bytes} bytes.", ("bytes", HIVE_CUSTOM_OP_DATA_MAX_LENGTH) );
  }
  FC_ASSERT( _db.has_hardfork( HIVE_HARDFORK_0_14__317 ) );

  if( _db.has_hardfork( HIVE_HARDFORK_0_20 ) )
  {
    FC_TODO( "Check if the following could become part of operation validation (unconditional)" );
    size_t num_auths = o.required_owner_auths.size() + o.required_active_auths.size() + o.required_posting_auths.size();
    for( const auto& auth : o.required_auths )
    {
      num_auths += auth.key_auths.size() + auth.account_auths.size();
    }

    FC_ASSERT( num_auths <= HIVE_MAX_AUTHORITY_MEMBERSHIP,
      "Authority membership exceeded. Max: ${max} Current: ${n}", ("max", HIVE_MAX_AUTHORITY_MEMBERSHIP)("n", num_auths) );
  }

  std::shared_ptr< custom_operation_interpreter > eval = _db.get_custom_json_evaluator( o.id );
  if( !eval )
    return;

  try
  {
    eval->apply( o );
  }
  catch( const fc::exception& e )
  {
    if( _db.is_in_control() )
      throw;
  }
  catch(...)
  {
    elog( "Unexpected exception applying custom json evaluator." );
  }
}


template<typename Operation>
void pow_apply( database& db, Operation o )
{
  const auto& dgp = db.get_dynamic_global_properties();

  if( db.has_hardfork( HIVE_HARDFORK_0_5__59 ) )
  {
    const auto& witness_by_work = db.get_index<witness_index>().indices().get<by_work>();
    auto work_itr = witness_by_work.find( o.work.work );
    if( work_itr != witness_by_work.end() )
    {
        FC_ASSERT( !"DUPLICATE WORK DISCOVERED", "${w}  ${witness}",("w",o)("wit",*work_itr) );
    }
  }

  const auto& accounts_by_name = db.get_index<account_index>().indices().get<by_name>();

  auto itr = accounts_by_name.find(o.get_worker_account());
  if(itr == accounts_by_name.end())
  {
    const auto& new_account = create_account( db, o.get_worker_account(), o.work.worker, dgp.time, true /*mined*/ );
    // ^ empty recovery account parameter means highest voted witness at time of recovery

#ifdef COLLECT_ACCOUNT_METADATA
    db.create< account_metadata_object >( [&]( account_metadata_object& meta )
    {
      meta.account = new_account.get_id();
    });
#else
    FC_UNUSED( new_account );
#endif

    db.create< account_authority_object >( [&]( account_authority_object& auth )
    {
      auth.account = o.get_worker_account();
      auth.owner = authority( 1, o.work.worker, 1);
      auth.active = auth.owner;
      auth.posting = auth.owner;
    });

    db.push_virtual_operation( account_created_operation(new_account.get_name(), o.get_worker_account(), asset(0, VESTS_SYMBOL), asset(0, VESTS_SYMBOL) ) );
  }

  const auto& worker_account = db.get_account( o.get_worker_account() ); // verify it exists
#ifndef HIVE_CONVERTER_BUILD // disable these checks, since there is a 2nd auth applied on all the accs in the alternate chain generated using hive blockchain converter
  const auto& worker_auth = db.get< account_authority_object, by_account >( o.get_worker_account() );
  FC_ASSERT( worker_auth.active.num_auths() == 1, "Miners can only have one key authority. ${a}", ("a",worker_auth.active) );
  FC_ASSERT( worker_auth.active.key_auths.size() == 1, "Miners may only have one key authority." );
  FC_ASSERT( worker_auth.active.key_auths.begin()->first == o.work.worker, "Work must be performed by key that signed the work." );
#endif
  FC_ASSERT( o.block_id == db.head_block_id(), "pow not for last block" );
  if( db.has_hardfork( HIVE_HARDFORK_0_13__256 ) )
    FC_ASSERT( worker_account.last_account_update < db.head_block_time(), "Worker account must not have updated their account this block." );

#ifndef HIVE_CONVERTER_BUILD // due to the optimization issues with blockchain_converter performing proof of work for every pow operations, this check is applied only in mainnet
  fc::sha256 target = db.get_pow_target();

  FC_ASSERT( o.work.work < target, "Work lacks sufficient difficulty." );
#endif

  db.modify( dgp, [&]( dynamic_global_property_object& p )
  {
    p.total_pow++; // make sure this doesn't break anything...
    p.num_pow_witnesses++;
  });


  const witness_object* cur_witness = db.find_witness( worker_account.get_name() );
  if( cur_witness ) {
    FC_ASSERT( cur_witness->pow_worker == 0, "This account is already scheduled for pow block production." );
    db.modify(*cur_witness, [&]( witness_object& w ){
        copy_legacy_chain_properties< true >( w.props, o.props );
        w.pow_worker        = dgp.total_pow;
        w.last_work         = o.work.work;
    });
  } else {
    db.create<witness_object>( [&]( witness_object& w )
    {
        w.owner             = o.get_worker_account();
        copy_legacy_chain_properties< true >( w.props, o.props );
        w.created           = db.head_block_time();
        w.signing_key       = o.work.worker;
        w.pow_worker        = dgp.total_pow;
        w.last_work         = o.work.work;
    });
  }
  /// POW reward depends upon whether we are before or after MINER_VOTING kicks in
  asset pow_reward = db.get_pow_reward();
  if( db.head_block_num() < HIVE_START_MINER_VOTING_BLOCK )
    pow_reward.amount *= HIVE_MAX_WITNESSES;
  db.adjust_supply( pow_reward, true );

  /// pay the witness that includes this POW
  const auto& inc_witness = db.get_account( dgp.current_witness );
  asset actual_reward;
  if( db.head_block_num() < HIVE_START_MINER_VOTING_BLOCK )
  {
    db.adjust_balance( inc_witness, pow_reward );
    actual_reward = pow_reward;
  }
  else
    actual_reward = db.create_vesting( inc_witness, pow_reward );
  db.push_virtual_operation( pow_reward_operation( dgp.current_witness, actual_reward ) );
}

void pow_evaluator::do_apply( const pow_operation& o ) {
  FC_ASSERT( !db().has_hardfork( HIVE_HARDFORK_0_13__256 ), "pow is deprecated. Use pow2 instead" );
  pow_apply( db(), o );
}


void pow2_evaluator::do_apply( const pow2_operation& o )
{
  database& db = this->db();
  FC_ASSERT( !db.has_hardfork( HIVE_HARDFORK_0_17__770 ), "mining is now disabled" );

  const auto& dgp = db.get_dynamic_global_properties();
#ifndef HIVE_CONVERTER_BUILD // due to the optimization issues with blockchain_converter performing proof of work for every pow operations, this check is applied only in mainnet
  uint32_t target_pow = db.get_pow_summary_target();
#endif
  account_name_type worker_account;

  if( db.has_hardfork( HIVE_HARDFORK_0_16__551 ) )
  {
    const auto& work = o.work.get< equihash_pow >();
    FC_ASSERT( work.prev_block == db.head_block_id(), "Equihash pow op not for last block" );
    auto recent_block_num = protocol::block_header::num_from_id( work.input.prev_block );
    FC_ASSERT( recent_block_num >= db.get_last_irreversible_block_num(),
      "Equihash pow done for block older than last irreversible block num. pow block: ${recent_block_num}, last irreversible block: ${lib}", (recent_block_num)("lib", db.get_last_irreversible_block_num()));
#ifndef HIVE_CONVERTER_BUILD
    FC_ASSERT( work.pow_summary < target_pow, "Insufficient work difficulty. Work: ${w}, Target: ${t}", ("w",work.pow_summary)("t", target_pow) );
#endif
    worker_account = work.input.worker_account;
  }
  else
  {
    const auto& work = o.work.get< pow2 >();
    FC_ASSERT( work.input.prev_block == db.head_block_id(), "Work not for last block" );

#ifndef HIVE_CONVERTER_BUILD
    FC_ASSERT( work.pow_summary < target_pow, "Insufficient work difficulty. Work: ${w}, Target: ${t}", ("w",work.pow_summary)("t", target_pow) );
#endif
    worker_account = work.input.worker_account;
  }

  FC_ASSERT( o.props.maximum_block_size >= HIVE_MIN_BLOCK_SIZE_LIMIT * 2, "Voted maximum block size is too small." );

  db.modify( dgp, [&]( dynamic_global_property_object& p )
  {
    p.total_pow++;
    p.num_pow_witnesses++;
  });

  const auto& accounts_by_name = db.get_index<account_index>().indices().get<by_name>();
  auto itr = accounts_by_name.find( worker_account );
  if(itr == accounts_by_name.end())
  {
    FC_ASSERT( o.new_owner_key.valid(), "New owner key is not valid." );
    const auto& new_account = create_account( db, worker_account, *o.new_owner_key, dgp.time, true /*mined*/ );
    // ^ empty recovery account parameter means highest voted witness at time of recovery

#ifdef COLLECT_ACCOUNT_METADATA
    db.create< account_metadata_object >( [&]( account_metadata_object& meta )
    {
      meta.account = new_account.get_id();
    });
#else
    FC_UNUSED( new_account );
#endif

    db.create< account_authority_object >( [&]( account_authority_object& auth )
    {
      auth.account = worker_account;
      auth.owner = authority( 1, *o.new_owner_key, 1);
      auth.active = auth.owner;
      auth.posting = auth.owner;
    });

    db.create<witness_object>( [&]( witness_object& w )
    {
        w.owner             = worker_account;
        copy_legacy_chain_properties< true >( w.props, o.props );
        w.created           = db.head_block_time();
        w.signing_key       = *o.new_owner_key;
        w.pow_worker        = dgp.total_pow;
    });

    _db.push_virtual_operation( account_created_operation(new_account.get_name(), worker_account, asset(0, VESTS_SYMBOL), asset(0, VESTS_SYMBOL) ) );
  }
  else
  {
    FC_ASSERT( !o.new_owner_key.valid(), "Cannot specify an owner key unless creating account." );
    const witness_object* cur_witness = db.find_witness( worker_account );
    FC_ASSERT( cur_witness, "Witness must be created for existing account before mining.");
    FC_ASSERT( cur_witness->pow_worker == 0, "This account is already scheduled for pow block production." );
    db.modify(*cur_witness, [&]( witness_object& w )
    {
        copy_legacy_chain_properties< true >( w.props, o.props );
        w.pow_worker        = dgp.total_pow;
    });
  }

  if( !db.has_hardfork( HIVE_HARDFORK_0_16__551) )
  {
    /// pay the witness that includes this POW
    asset inc_reward = db.get_pow_reward();
    db.adjust_supply( inc_reward, true );

    const auto& inc_witness = db.get_account( dgp.current_witness );
    asset actual_reward = db.create_vesting( inc_witness, inc_reward );
    db.push_virtual_operation( pow_reward_operation( dgp.current_witness, actual_reward ) );
  }
}

void feed_publish_evaluator::do_apply( const feed_publish_operation& o )
{
  if( _db.has_hardfork( HIVE_HARDFORK_0_20__409 ) )
    FC_ASSERT( is_asset_type( o.exchange_rate.base, HBD_SYMBOL ) && is_asset_type( o.exchange_rate.quote, HIVE_SYMBOL ),
        "Price feed must be a HBD/HIVE price" );

  const auto& witness = _db.get_witness( o.publisher );
  _db.modify( witness, [&]( witness_object& w )
  {
    w.hbd_exchange_rate = o.exchange_rate;
    w.last_hbd_exchange_update = _db.head_block_time();
  });
}

void convert_evaluator::do_apply( const convert_operation& o )
{
  const auto& owner = _db.get_account( o.owner );
  _db.adjust_balance( owner, -o.amount );

  const auto& fhistory = _db.get_feed_history();
  FC_ASSERT( !fhistory.current_median_history.is_null(), "Cannot convert HBD because there is no price feed." );

  auto hive_conversion_delay = HIVE_CONVERSION_DELAY_PRE_HF_16;
  if( _db.has_hardfork( HIVE_HARDFORK_0_16__551) )
    hive_conversion_delay = HIVE_CONVERSION_DELAY;

  _db.create<convert_request_object>( owner, o.amount, _db.head_block_time() + hive_conversion_delay, o.requestid );
}

void collateralized_convert_evaluator::do_apply( const collateralized_convert_operation& o )
{
  FC_ASSERT( _db.has_hardfork( HIVE_HARDFORK_1_25 ), "Operation not available until HF25" );

  const auto& owner = _db.get_account( o.owner );
  _db.adjust_balance( owner, -o.amount );

  const auto& fhistory = _db.get_feed_history();
  FC_ASSERT( !fhistory.current_median_history.is_null(), "Cannot convert HIVE because there is no price feed." );
  const auto& dgpo = _db.get_dynamic_global_properties();
  FC_ASSERT( dgpo.hbd_print_rate > 0, "Creation of new HBD blocked at this time due to global limit." );
    //note that feed and therefore print rate is updated once per hour, so without above check there could be
    //enough room for new HBD, but there is a chance the official price would still be artificial (artificial
    //price is not used in this conversion, but users might think it is - better to stop them from making mistake)

  //if you change something here take a look at wallet_api::estimate_hive_collateral as well

  //cut amount by collateral ratio
  uint128_t _amount = ( uint128_t( o.amount.amount.value ) * HIVE_100_PERCENT ) / HIVE_CONVERSION_COLLATERAL_RATIO;
  asset for_immediate_conversion = asset( fc::uint128_to_uint64(_amount), o.amount.symbol );

  //immediately create HBD - apply fee to current rolling minimum price
  auto converted_amount = multiply_with_fee( for_immediate_conversion, fhistory.current_min_history,
    HIVE_COLLATERALIZED_CONVERSION_FEE, HIVE_SYMBOL );
  FC_ASSERT( converted_amount.amount > 0, "Amount of collateral too low - conversion gives no HBD" );
  _db.adjust_balance( owner, converted_amount );

  _db.modify( dgpo, [&]( dynamic_global_property_object& p )
  {
    //HIVE supply (and virtual supply in part related to HIVE) will be corrected after actual conversion
    p.current_hbd_supply += converted_amount;
    p.virtual_supply += converted_amount * fhistory.current_median_history;
  } );
  //note that we created new HBD out of thin air and we will burn the related HIVE when actual conversion takes
  //place; there is alternative approach - we could burn all collateral now and print back excess when we are
  //to return it to the user; the difference slightly changes calculations below, that is, currently we might
  //allow slightly more HBD to be printed

  uint16_t percent_hbd = _db.calculate_HBD_percent();
  FC_ASSERT( percent_hbd <= dgpo.hbd_stop_percent, "Creation of new ${hbd} violates global limit.", ( "hbd", converted_amount ) );

  _db.create<collateralized_convert_request_object>( owner, o.amount, converted_amount,
    _db.head_block_time() + HIVE_COLLATERALIZED_CONVERSION_DELAY, o.requestid );

  _db.push_virtual_operation( collateralized_convert_immediate_conversion_operation( o.owner, o.requestid, converted_amount ) );
}

void limit_order_create_evaluator::do_apply( const limit_order_create_operation& o )
{
  FC_ASSERT( o.expiration > _db.head_block_time(), "Limit order has to expire after head block time." );

  time_point_sec expiration = o.expiration;
  FC_TODO( "Check past order expirations and cleanup after HF 20" )
  if( _db.has_hardfork( HIVE_HARDFORK_0_20__1449 ) )
  {
    FC_ASSERT( o.expiration <= _db.head_block_time() + HIVE_MAX_LIMIT_ORDER_EXPIRATION, "Limit Order Expiration must not be more than 28 days in the future" );
  }
#ifndef HIVE_CONVERTER_BUILD // limit_order_object expiration time is explicitly set during the conversion time before HF20, due to the altered block id
  else
  {
    uint32_t rand_offset = _db.head_block_id()._hash[ 4 ] % 86400;
    expiration = std::min( o.expiration, fc::time_point_sec( HIVE_HARDFORK_0_20_TIME + HIVE_MAX_LIMIT_ORDER_EXPIRATION + rand_offset ) );
  }
#endif

  _db.adjust_balance( o.owner, -o.amount_to_sell );
  const auto& order = _db.create<limit_order_object>( o.owner, o.amount_to_sell, o.get_price(), _db.head_block_time(), expiration, o.orderid );

  bool filled = _db.apply_order( order );

  if( o.fill_or_kill ) FC_ASSERT( filled, "Cancelling order because it was not filled." );
}

void limit_order_create2_evaluator::do_apply( const limit_order_create2_operation& o )
{
  FC_ASSERT( o.expiration > _db.head_block_time(), "Limit order has to expire after head block time." );

  time_point_sec expiration = o.expiration;
  FC_TODO( "Check past order expirations and cleanup after HF 20" )
  if( _db.has_hardfork( HIVE_HARDFORK_0_20__1449 ) )
  {
    FC_ASSERT( o.expiration <= _db.head_block_time() + HIVE_MAX_LIMIT_ORDER_EXPIRATION, "Limit Order Expiration must not be more than 28 days in the future" );
  }
  else
  {
    expiration = std::min( o.expiration, fc::time_point_sec( HIVE_HARDFORK_0_20_TIME + HIVE_MAX_LIMIT_ORDER_EXPIRATION ) );
  }

  _db.adjust_balance( o.owner, -o.amount_to_sell );
  const auto& order = _db.create<limit_order_object>( o.owner, o.amount_to_sell, o.exchange_rate, _db.head_block_time(), expiration, o.orderid );

  bool filled = _db.apply_order( order );

  if( o.fill_or_kill ) FC_ASSERT( filled, "Cancelling order because it was not filled." );
}

void limit_order_cancel_evaluator::do_apply( const limit_order_cancel_operation& o )
{
  _db.cancel_order( _db.get_limit_order( o.owner, o.orderid ) );
}

void claim_account_evaluator::do_apply( const claim_account_operation& o )
{
  FC_ASSERT( _db.has_hardfork( HIVE_HARDFORK_0_20__1771 ), "claim_account_operation is not enabled until hardfork 20." );

  const auto& creator = _db.get_account( o.creator );
  const auto& wso = _db.get_witness_schedule_object();

  FC_ASSERT( creator.get_balance() >= o.fee, "Insufficient balance to create account.", ( "creator.balance", creator.get_balance() )( "required", o.fee ) );

  if( o.fee.amount == 0 )
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
      });
    }

    FC_ASSERT( gpo.available_account_subsidies >= HIVE_ACCOUNT_SUBSIDY_PRECISION, "There are not enough subsidized accounts to claim" );

    _db.modify( gpo, [&]( dynamic_global_property_object& gpo )
    {
      gpo.available_account_subsidies -= HIVE_ACCOUNT_SUBSIDY_PRECISION;
    });
  }
  else
  {
    FC_ASSERT( o.fee == wso.median_props.account_creation_fee,
      "Cannot pay more than account creation fee. paid: ${p} fee: ${f}",
      ("p", o.fee.amount.value)
      ("f", wso.median_props.account_creation_fee) );
  }

  _db.adjust_balance( _db.get_account( HIVE_NULL_ACCOUNT ), o.fee );

  _db.modify( creator, [&]( account_object& a )
  {
    a.balance -= o.fee;
    a.pending_claimed_accounts++;
  });
}

void create_claimed_account_evaluator::do_apply( const create_claimed_account_operation& o )
{
  FC_ASSERT( _db.has_hardfork( HIVE_HARDFORK_0_20__1771 ), "create_claimed_account_operation is not enabled until hardfork 20." );

  const auto& creator = _db.get_account( o.creator );
  const auto& props = _db.get_dynamic_global_properties();

  FC_ASSERT( creator.pending_claimed_accounts > 0, "${creator} has no claimed accounts to create", ( "creator", o.creator ) );

  verify_authority_accounts_exist( _db, o.owner, o.new_account_name, authority::owner );
  verify_authority_accounts_exist( _db, o.active, o.new_account_name, authority::active );
  verify_authority_accounts_exist( _db, o.posting, o.new_account_name, authority::posting );

  _db.modify( creator, [&]( account_object& a )
  {
    a.pending_claimed_accounts--;
  });

  const auto& new_account = create_account( _db, o.new_account_name, o.memo_key, props.time, false /*mined*/, &creator );

#ifdef COLLECT_ACCOUNT_METADATA
  _db.create< account_metadata_object >( [&]( account_metadata_object& meta )
  {
    meta.account = new_account.get_id();
    from_string( meta.json_metadata, o.json_metadata );
  });
#else
  FC_UNUSED( new_account );
#endif

  _db.create< account_authority_object >( [&]( account_authority_object& auth )
  {
    auth.account = o.new_account_name;
    auth.owner = o.owner;
    auth.active = o.active;
    auth.posting = o.posting;
    auth.previous_owner_update = fc::time_point_sec::min();
    auth.last_owner_update = fc::time_point_sec::min();
  });

  _db.push_virtual_operation( account_created_operation(new_account.get_name(), o.creator, asset(0, VESTS_SYMBOL), asset(0, VESTS_SYMBOL) ) );
}

void request_account_recovery_evaluator::do_apply( const request_account_recovery_operation& o )
{
  const auto& account_to_recover = _db.get_account( o.account_to_recover );

  if ( account_to_recover.has_recovery_account() ) // Make sure recovery matches expected recovery account
  {
    const auto& recovery_account = _db.get_account( account_to_recover.get_recovery_account() );
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

  if( request == recovery_request_idx.end() ) // New Request
  {
    FC_ASSERT( !o.new_owner_authority.is_impossible(), "Cannot recover using an impossible authority." );
    FC_ASSERT( o.new_owner_authority.weight_threshold, "Cannot recover using an open authority." );

    if( _db.has_hardfork( HIVE_HARDFORK_0_20 ) )
    {
      validate_auth_size( o.new_owner_authority );
    }

    // Check accounts in the new authority exist
    if( ( _db.has_hardfork( HIVE_HARDFORK_0_15__465 ) ) )
    {
      for( auto& a : o.new_owner_authority.account_auths )
      {
        _db.get_account( a.first );
      }
    }

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
    if( ( _db.has_hardfork( HIVE_HARDFORK_0_15__465 ) ) )
    {
      for( auto& a : o.new_owner_authority.account_auths )
      {
        _db.get_account( a.first );
      }
    }

    _db.modify( *request, [&]( account_recovery_request_object& req )
    {
      req.set_new_owner_authority( o.new_owner_authority, _db.head_block_time() + HIVE_ACCOUNT_RECOVERY_REQUEST_EXPIRATION_PERIOD );
    });
  }
}

void recover_account_evaluator::do_apply( const recover_account_operation& o )
{
  const auto& account = _db.get_account( o.account_to_recover );

  if( _db.has_hardfork( HIVE_HARDFORK_0_12 ) )
    FC_ASSERT( util::owner_update_limit_mgr::check( _db.head_block_time(), account.get_last_account_recovery_time() ), "${m}", ("m", util::owner_update_limit_mgr::msg( _db.has_hardfork( HIVE_HARDFORK_1_26_AUTH_UPDATE ) ) ) );

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
  _db.modify( account, [&]( account_object& a )
  {
    a.set_last_account_recovery_time( _db.head_block_time() );
  });
}

void change_recovery_account_evaluator::do_apply( const change_recovery_account_operation& o )
{
  const auto& new_recovery_account = _db.get_account( o.new_recovery_account ); // validate account exists
    //ABW: can't clear existing recovery agent to set it to top voted witness
  const auto& account_to_recover = _db.get_account( o.account_to_recover );

  const auto& change_recovery_idx = _db.get_index< change_recovery_account_request_index, by_account >();
  auto request = change_recovery_idx.find( o.account_to_recover );

  if( request == change_recovery_idx.end() ) // New request
  {
    //ABW: it is possible to request change to currently set recovery agent (empty operation)
    _db.create< change_recovery_account_request_object >( account_to_recover, new_recovery_account, _db.head_block_time() + HIVE_OWNER_AUTH_RECOVERY_PERIOD );
  }
  else if( account_to_recover.get_recovery_account() != new_recovery_account.get_id() ) // Change existing request
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

void transfer_to_savings_evaluator::do_apply( const transfer_to_savings_operation& op )
{
  const auto& from = _db.get_account( op.from );
  const auto& to   = _db.get_account(op.to);

  if( _db.has_hardfork( HIVE_HARDFORK_0_21__3343 ) )
  {
    FC_ASSERT( !_db.is_treasury( op.to ), "Cannot transfer savings to ${s}", ("s", op.to ) );
  }

  _db.adjust_balance( from, -op.amount );
  _db.adjust_savings_balance( to, op.amount );
}

void transfer_from_savings_evaluator::do_apply( const transfer_from_savings_operation& op )
{
  const auto& from = _db.get_account( op.from );
  _db.get_account(op.to); // Verify to account exists

  FC_ASSERT( from.savings_withdraw_requests < HIVE_SAVINGS_WITHDRAW_REQUEST_LIMIT, "Account has reached limit for pending withdraw requests." );

  if( _db.has_hardfork( HIVE_HARDFORK_0_21__3343 ) )
  {
    FC_ASSERT( op.amount.symbol == HBD_SYMBOL || !_db.is_treasury( op.to ), "Can only transfer HBD to ${s}", ("s", op.to ) );
  }

  FC_ASSERT( _db.get_savings_balance( from, op.amount.symbol ) >= op.amount );
  _db.adjust_savings_balance( from, -op.amount );
  _db.create<savings_withdraw_object>( op.from, op.to, op.amount, op.memo, _db.head_block_time() + HIVE_SAVINGS_WITHDRAW_TIME, op.request_id );

  _db.modify( from, [&]( account_object& a )
  {
    a.savings_withdraw_requests++;
  });
}

void cancel_transfer_from_savings_evaluator::do_apply( const cancel_transfer_from_savings_operation& op )
{
  const auto& swo = _db.get_savings_withdraw( op.from, op.request_id );
  _db.adjust_savings_balance( _db.get_account( swo.from ), swo.amount );
  _db.remove( swo );

  const auto& from = _db.get_account( op.from );
  _db.modify( from, [&]( account_object& a )
  {
    a.savings_withdraw_requests--;
  });
}

void decline_voting_rights_evaluator::do_apply( const decline_voting_rights_operation& o )
{
  FC_ASSERT( _db.has_hardfork( HIVE_HARDFORK_0_14__324 ) );

  const auto& account = _db.get_account( o.account );

  if( _db.is_in_control() || _db.has_hardfork( HIVE_HARDFORK_1_28 ) )
    FC_ASSERT( account.can_vote, "Voter declined voting rights already, therefore trying to decline voting rights again is forbidden." );

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
  FC_ASSERT( false, "Reset Account Operation is currently disabled." );
  //ABW: see discussion in https://github.com/steemit/steem/issues/240
  //apparently the idea was never put in active use and it does not seem it ever will
  //related member of account_object was removed as it was taking space with no purpose
/*
  const auto& acnt = _db.get_account( op.account_to_reset );
  auto band = _db.find< account_bandwidth_object, by_account_bandwidth_type >( boost::make_tuple( op.account_to_reset, bandwidth_type::old_forum ) );
  if( band != nullptr )
    FC_ASSERT( ( _db.head_block_time() - band->last_bandwidth_update ) > fc::days(60), "Account must be inactive for 60 days to be eligible for reset" );
  FC_ASSERT( acnt.reset_account == op.reset_account, "Reset account does not match reset account on account." );

  _db.update_owner_authority( acnt, op.new_owner_authority );
*/
}

void set_reset_account_evaluator::do_apply( const set_reset_account_operation& op )
{
  FC_ASSERT( false, "Set Reset Account Operation is currently disabled." );
  //related to reset_account_operation
/*
  const auto& acnt = _db.get_account( op.account );
  _db.get_account( op.reset_account );

  FC_ASSERT( acnt.reset_account == op.current_reset_account, "Current reset account does not match reset account on account." );
  FC_ASSERT( acnt.reset_account != op.reset_account, "Reset account must change" );

  _db.modify( acnt, [&]( account_object& a )
  {
      a.reset_account = op.reset_account;
  });
*/
}

void claim_reward_balance_evaluator::do_apply( const claim_reward_balance_operation& op )
{
  const auto& acnt = _db.get_account( op.account );

  FC_ASSERT( op.reward_hive <= acnt.get_rewards(), "Cannot claim that much HIVE. Claim: ${c} Actual: ${a}",
    ("c", op.reward_hive)("a", acnt.get_rewards() ) );
  FC_ASSERT( op.reward_hbd <= acnt.get_hbd_rewards(), "Cannot claim that much HBD. Claim: ${c} Actual: ${a}",
    ("c", op.reward_hbd)("a", acnt.get_hbd_rewards()) );
  FC_ASSERT( op.reward_vests <= acnt.get_vest_rewards(), "Cannot claim that much VESTS. Claim: ${c} Actual: ${a}",
    ("c", op.reward_vests)("a", acnt.get_vest_rewards() ) );

  asset reward_vesting_hive_to_move = asset( 0, HIVE_SYMBOL );
  if( op.reward_vests == acnt.get_vest_rewards() )
    reward_vesting_hive_to_move = acnt.get_vest_rewards_as_hive();
  else
    reward_vesting_hive_to_move = asset( fc::uint128_to_uint64( ( uint128_t( op.reward_vests.amount.value ) * uint128_t( acnt.get_vest_rewards_as_hive().amount.value ) )
      / uint128_t( acnt.get_vest_rewards().amount.value ) ), HIVE_SYMBOL );

  _db.adjust_reward_balance( acnt, -op.reward_hive );
  _db.adjust_reward_balance( acnt, -op.reward_hbd );
  _db.adjust_balance( acnt, op.reward_hive );
  _db.adjust_balance( acnt, op.reward_hbd );

  _db.modify( acnt, [&]( account_object& a )
  {
    if( _db.has_hardfork( HIVE_HARDFORK_0_20__2539 ) )
    {
      util::update_manabar( _db.get_dynamic_global_properties(), a, op.reward_vests.amount.value );
    }

    a.vesting_shares += op.reward_vests;
    a.reward_vesting_balance -= op.reward_vests;
    a.reward_vesting_hive -= reward_vesting_hive_to_move;
  });

  _db.modify( _db.get_dynamic_global_properties(), [&]( dynamic_global_property_object& gpo )
  {
    gpo.total_vesting_shares += op.reward_vests;
    gpo.total_vesting_fund_hive += reward_vesting_hive_to_move;

    gpo.pending_rewarded_vesting_shares -= op.reward_vests;
    gpo.pending_rewarded_vesting_hive -= reward_vesting_hive_to_move;
  });

  _db.adjust_proxied_witness_votes( acnt, op.reward_vests.amount );
}

#ifdef HIVE_ENABLE_SMT
void claim_reward_balance2_evaluator::do_apply( const claim_reward_balance2_operation& op )
{
  const account_object* a = nullptr; // Lazily initialized below because it may turn out unnecessary.

  for( const asset& token : op.reward_tokens )
  {
    if( token.amount == 0 )
      continue;

    if( token.symbol.space() == asset_symbol_type::smt_nai_space )
    {
      _db.adjust_reward_balance( op.account, -token );
      _db.adjust_balance( op.account, token );
    }
    else
    {
      // Lazy init here.
      if( a == nullptr )
      {
        a = _db.find_account( op.account );
        FC_ASSERT( a != nullptr, "Could NOT find account ${a}", ("a", op.account) );
      }

      if( token.symbol == VESTS_SYMBOL)
      {
        FC_ASSERT( token <= a->get_vest_rewards(), "Cannot claim that much VESTS. Claim: ${c} Actual: ${a}",
          ("c", token)("a", a->get_vest_rewards() ) );

        asset reward_vesting_hive_to_move = asset( 0, HIVE_SYMBOL );
        if( token == a->get_vest_rewards() )
          reward_vesting_hive_to_move = a->get_vest_rewards_as_hive();
        else
          reward_vesting_hive_to_move = asset( fc::uint128_to_uint64( ( uint128_t( token.amount.value ) * uint128_t( a->get_vest_rewards_as_hive().amount.value ) )
            / uint128_t( a->get_vest_rewards().amount.value ) ), HIVE_SYMBOL );

        _db.modify( *a, [&]( account_object& a )
        {
          a.vesting_shares += token;
          a.reward_vesting_balance -= token;
          a.reward_vesting_hive -= reward_vesting_hive_to_move;
        });

        _db.modify( _db.get_dynamic_global_properties(), [&]( dynamic_global_property_object& gpo )
        {
          gpo.total_vesting_shares += token;
          gpo.total_vesting_fund_hive += reward_vesting_hive_to_move;

          gpo.pending_rewarded_vesting_shares -= token;
          gpo.pending_rewarded_vesting_hive -= reward_vesting_hive_to_move;
        });

        _db.adjust_proxied_witness_votes( *a, token.amount );
      }
      else if( token.symbol == HIVE_SYMBOL || token.symbol == HBD_SYMBOL )
      {
        FC_ASSERT( is_asset_type( token, HIVE_SYMBOL ) == false || token <= a->get_rewards(),
                "Cannot claim that much HIVE. Claim: ${c} Actual: ${a}", ("c", token)("a", a->get_rewards() ) );
        FC_ASSERT( is_asset_type( token, HBD_SYMBOL ) == false || token <= a->get_hbd_rewards(),
                "Cannot claim that much HBD. Claim: ${c} Actual: ${a}", ("c", token)("a", a->get_hbd_rewards()) );
        _db.adjust_reward_balance( *a, -token );
        _db.adjust_balance( *a, token );
      }
      else
        FC_ASSERT( false, "Unknown asset symbol" );
    } // non-SMT token
  } // for( const auto& token : op.reward_tokens )
}
#endif

void delegate_vesting_shares_evaluator::do_apply( const delegate_vesting_shares_operation& op )
{
FC_TODO("Update get_effective_vesting_shares when modifying this operation to support SMTs." )

  const auto& delegator = _db.get_account( op.delegator );
  const auto& delegatee = _db.get_account( op.delegatee );
  auto* delegation = _db.find< vesting_delegation_object, by_delegation >( boost::make_tuple( delegator.get_id(), delegatee.get_id() ) );

  const auto& gpo = _db.get_dynamic_global_properties();

  asset available_shares;
  asset available_downvote_shares;

  if( _db.has_hardfork( HIVE_HARDFORK_0_20__2539 ) )
  {
    auto max_mana = delegator.get_effective_vesting_shares();

    _db.modify( delegator, [&]( account_object& a )
    {
      util::update_manabar( gpo, a );
    });

    available_shares = asset( delegator.voting_manabar.current_mana, VESTS_SYMBOL );
    if( gpo.downvote_pool_percent )
    {
      if( _db.has_hardfork( HIVE_HARDFORK_0_22__3485 ) )
      {
        available_downvote_shares = asset(
          fc::uint128_to_int64( ( uint128_t( delegator.downvote_manabar.current_mana ) * HIVE_100_PERCENT ) / gpo.downvote_pool_percent
          + ( HIVE_100_PERCENT / gpo.downvote_pool_percent ) - 1 ), VESTS_SYMBOL );
      }
      else
      {
        available_downvote_shares = asset(
          ( delegator.downvote_manabar.current_mana * HIVE_100_PERCENT ) / gpo.downvote_pool_percent
          + ( HIVE_100_PERCENT / gpo.downvote_pool_percent ) - 1, VESTS_SYMBOL );
      }
    }
    else
    {
      available_downvote_shares = available_shares;
    }

    // Assume delegated VESTS are used first when consuming mana. You cannot delegate received vesting shares
    available_shares.amount = std::min( available_shares.amount, max_mana - delegator.received_vesting_shares.amount );
    available_downvote_shares.amount = std::min( available_downvote_shares.amount, max_mana - delegator.received_vesting_shares.amount );

    if( delegator.next_vesting_withdrawal < fc::time_point_sec::maximum()
      && delegator.to_withdraw.amount - delegator.withdrawn.amount > delegator.vesting_withdraw_rate.amount )
    {
      /*
      current voting mana does not include the current week's power down:

      std::min(
        account.vesting_withdraw_rate,           // Weekly amount
        account.to_withdraw - account.withdrawn  // Or remainder
        );

      But an account cannot delegate **any** VESTS that they are powering down.
      The remaining withdrawal needs to be added in but then the current week is double counted.
      */

      auto weekly_withdraw = asset( std::min(
        delegator.vesting_withdraw_rate.amount,                   // Weekly amount
        delegator.to_withdraw.amount - delegator.withdrawn.amount // Or remainder
        ), VESTS_SYMBOL );

      available_shares += weekly_withdraw - asset( delegator.to_withdraw.amount - delegator.withdrawn.amount, VESTS_SYMBOL );
      available_downvote_shares += weekly_withdraw - asset( delegator.to_withdraw.amount - delegator.withdrawn.amount, VESTS_SYMBOL );
    }
  }
  else
  {
    available_shares = delegator.get_vesting().to_asset() - delegator.delegated_vesting_shares - asset( delegator.to_withdraw.amount - delegator.withdrawn.amount, VESTS_SYMBOL );
  }

  const auto& wso = _db.get_witness_schedule_object();

  // HF 20 increase fee meaning by 30x, reduce these thresholds to compensate.
  auto min_delegation = _db.has_hardfork( HIVE_HARDFORK_0_20__1761 ) ?
    asset( wso.median_props.account_creation_fee.amount / 3, HIVE_SYMBOL ) * gpo.get_vesting_share_price() :
    asset( wso.median_props.account_creation_fee.amount * 10, HIVE_SYMBOL ) * gpo.get_vesting_share_price();
  auto min_update = _db.has_hardfork( HIVE_HARDFORK_0_20__1761 ) ?
    asset( wso.median_props.account_creation_fee.amount / 30, HIVE_SYMBOL ) * gpo.get_vesting_share_price() :
    wso.median_props.account_creation_fee * gpo.get_vesting_share_price();

  // If delegation doesn't exist, create it
  if( delegation == nullptr )
  {
    FC_ASSERT( available_shares >= op.vesting_shares, "Account ${acc} does not have enough mana to delegate. required: ${r} available: ${a}",
      ("acc", op.delegator)("r", op.vesting_shares)("a", available_shares) );
    FC_TODO( "This hardfork check should not be needed. Remove after HF21 if that is the case." );
    if( _db.has_hardfork( HIVE_HARDFORK_0_21__3336 ) )
      FC_ASSERT( available_downvote_shares >= op.vesting_shares, "Account ${acc} does not have enough downvote mana to delegate. required: ${r} available: ${a}",
      ("acc", op.delegator)("r", op.vesting_shares)("a", available_downvote_shares) );
    FC_ASSERT( op.vesting_shares >= min_delegation, "Account must delegate a minimum of ${v}", ("v", min_delegation) );

    _db.create< vesting_delegation_object >( delegator, delegatee, op.vesting_shares, _db.head_block_time() );

    _db.modify( delegator, [&]( account_object& a )
    {
      a.delegated_vesting_shares += op.vesting_shares;

      if( _db.has_hardfork( HIVE_HARDFORK_0_20__2539 ) )
      {
        a.voting_manabar.use_mana( op.vesting_shares.amount.value );

        if( _db.has_hardfork( HIVE_HARDFORK_0_22__3485 ) )
        {
          a.downvote_manabar.use_mana( fc::uint128_to_int64( ( uint128_t( op.vesting_shares.amount.value ) * gpo.downvote_pool_percent ) / HIVE_100_PERCENT ) );
        }
        else if( _db.has_hardfork( HIVE_HARDFORK_0_21__3336 ) )
        {
          a.downvote_manabar.use_mana( op.vesting_shares.amount.value );
        }
      }
    });

    _db.modify( delegatee, [&]( account_object& a )
    {
      if( _db.has_hardfork( HIVE_HARDFORK_0_20__2539 ) )
      {
        util::update_manabar( gpo, a, op.vesting_shares.amount.value );
      }

      a.received_vesting_shares += op.vesting_shares;
    });
  }
  // Else if the delegation is increasing
  else if( op.vesting_shares >= delegation->get_vesting() )
  {
    auto delta = op.vesting_shares - delegation->get_vesting();

    FC_ASSERT( delta >= min_update, "Hive Power increase is not enough of a difference. min_update: ${min}", ("min", min_update) );
    FC_ASSERT( available_shares >= delta, "Account ${acc} does not have enough mana to delegate. required: ${r} available: ${a}",
      ("acc", op.delegator)("r", delta)("a", available_shares) );
    FC_TODO( "This hardfork check should not be needed. Remove after HF21 if that is the case." );
    if( _db.has_hardfork( HIVE_HARDFORK_0_21__3336 ) )
      FC_ASSERT( available_downvote_shares >= delta, "Account ${acc} does not have enough downvote mana to delegate. required: ${r} available: ${a}",
      ("acc", op.delegator)("r", delta)("a", available_downvote_shares) );

    _db.modify( delegator, [&]( account_object& a )
    {
      a.delegated_vesting_shares += delta;

      if( _db.has_hardfork( HIVE_HARDFORK_0_20__2539 ) )
      {
        a.voting_manabar.use_mana( delta.amount.value );

        if( _db.has_hardfork( HIVE_HARDFORK_0_22__3485 ) )
        {
          a.downvote_manabar.use_mana( fc::uint128_to_int64( ( uint128_t( delta.amount.value ) * gpo.downvote_pool_percent ) / HIVE_100_PERCENT ) );
        }
        else if( _db.has_hardfork( HIVE_HARDFORK_0_21__3336 ) )
        {
          a.downvote_manabar.use_mana( delta.amount.value );
        }
      }
    });

    _db.modify( delegatee, [&]( account_object& a )
    {
      if( _db.has_hardfork( HIVE_HARDFORK_0_20__2539 ) )
      {
        util::update_manabar( gpo, a, delta.amount.value );
      }

      a.received_vesting_shares += delta;
    });

    _db.modify( *delegation, [&]( vesting_delegation_object& obj )
    {
      obj.set_vesting( op.vesting_shares );
    });
  }
  // Else the delegation is decreasing
  else
  {
    auto delta = delegation->get_vesting() - op.vesting_shares;

    if( op.vesting_shares.amount > 0 )
    {
      FC_ASSERT( delta >= min_update, "Hive Power decrease is not enough of a difference. min_update: ${min}", ("min", min_update) );
      FC_ASSERT( op.vesting_shares >= min_delegation, "Delegation must be removed or leave minimum delegation amount of ${v}", ("v", min_delegation) );
    }
    else
    {
      FC_ASSERT( delegation->get_vesting().amount > 0, "Delegation would set vesting_shares to zero, but it is already zero");
    }

    _db.create< vesting_delegation_expiration_object >( delegator, delta,
      std::max( _db.head_block_time() + gpo.delegation_return_period, delegation->get_min_delegation_time() ) );

    _db.modify( delegatee, [&]( account_object& a )
    {
      if( _db.has_hardfork( HIVE_HARDFORK_0_22__3485 ) )
      {
        util::update_manabar( gpo, a );
      }

      a.received_vesting_shares -= delta;

      if( _db.has_hardfork( HIVE_HARDFORK_0_20__2539 ) )
      {
        a.voting_manabar.use_mana( delta.amount.value );

        if( _db.has_hardfork( HIVE_HARDFORK_0_21__3336 ) )
        {
          if( _db.has_hardfork( HIVE_HARDFORK_0_22__3485 ) )
          {
            a.downvote_manabar.use_mana( fc::uint128_to_int64( ( uint128_t( delta.amount.value ) * gpo.downvote_pool_percent ) / HIVE_100_PERCENT ) );
          }
          else
          {
            a.downvote_manabar.use_mana( op.vesting_shares.amount.value );
          }
        }
      }
    });

    if( op.vesting_shares.amount > 0 )
    {
      _db.modify( *delegation, [&]( vesting_delegation_object& obj )
      {
        obj.set_vesting( op.vesting_shares );
      });
    }
    else
    {
      _db.remove( *delegation );
    }
  }
}


void recurrent_transfer_evaluator::do_apply( const recurrent_transfer_operation& op )
{
  FC_ASSERT( _db.has_hardfork( HIVE_HARDFORK_1_25 ), "Recurrent transfers are not enabled until hardfork ${hf}", ("hf", HIVE_HARDFORK_1_25) );

  const auto& from_account = _db.get_account(op.from );
  const auto& to_account = _db.get_account( op.to );

  asset available = _db.get_balance( from_account, op.amount.symbol );
  FC_ASSERT( available >= op.amount, "Account does not have enough tokens for the first transfer, has ${has} needs ${needs}", ("has",  available)("needs", op.amount) );

  const auto& rt_idx = _db.get_index< recurrent_transfer_index, by_from_to_id >();
  auto itr = rt_idx.find(boost::make_tuple(from_account.get_id(), to_account.get_id()));

  if( itr == rt_idx.end() )
  {
    FC_ASSERT( from_account.open_recurrent_transfers < HIVE_MAX_OPEN_RECURRENT_TRANSFERS, "Account can't have more than ${rt} recurrent transfers", ( "rt", HIVE_MAX_OPEN_RECURRENT_TRANSFERS ) );
    // If the recurrent transfer is not found and the amount is 0 it means the user wants to delete a transfer that doesnt exists
    FC_ASSERT( op.amount.amount != 0, "Cannot create a recurrent transfer with 0 amount");
    _db.create< recurrent_transfer_object >(_db.head_block_time(), from_account, to_account, op.amount, op.memo, op.recurrence, op.executions);

    _db.modify(from_account, [](account_object& a )
    {
      a.open_recurrent_transfers++;
    });
  }
  else if( op.amount.amount == 0 )
  {
    _db.remove( *itr );
    _db.modify(from_account, [&](account_object& a )
    {
      FC_ASSERT( a.open_recurrent_transfers > 0 );
      a.open_recurrent_transfers--;
    });
  }
  else
  {
    _db.modify( *itr, [&]( recurrent_transfer_object& rt )
    {
      rt.amount = op.amount;
      from_string( rt.memo, op.memo );
      rt.set_recurrence_trigger_date(_db.head_block_time(), op.recurrence);
      rt.remaining_executions = op.executions;
    });
  }
}

void witness_block_approve_evaluator::do_apply(const witness_block_approve_operation& op)
{
  // This transaction si /updait's handled in database::process_fast_confirm_transaction
  // and never reaches the 
  FC_ASSERT(false, "This operation may not be included in a block");
}

} } // hive::chain
