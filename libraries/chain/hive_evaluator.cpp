#include <hive/chain/hive_fwd.hpp>

#include <hive/protocol/fixed_string.hpp>

#include <hive/chain/hive_evaluator.hpp>
#include <hive/chain/database.hpp>
#include <hive/chain/database_virtual_operations.hpp>
#include <hive/chain/custom_operation_interpreter.hpp>
#include <hive/chain/account_object_multiindex.hpp>
#include <hive/chain/global_property_object_multiindex.hpp>
#include <hive/chain/block_summary_object.hpp>
#include <hive/chain/witness_objects_multiindex.hpp>
#include <hive/chain/evaluator_registry.hpp>

#include <fc/macros.hpp>

#include <fc/uint128.hpp>

#include <boost/scope_exit.hpp>

#include <limits>

namespace hive { namespace chain {
  using fc::uint128_t;

HIVE_DEFINE_EVALUATOR( witness_update )
HIVE_DEFINE_EVALUATOR( witness_set_properties )
HIVE_DEFINE_EVALUATOR( account_witness_proxy )
HIVE_DEFINE_EVALUATOR( account_witness_vote )
HIVE_DEFINE_EVALUATOR( custom )
HIVE_DEFINE_EVALUATOR( custom_json )
HIVE_DEFINE_EVALUATOR( custom_binary )
HIVE_DEFINE_EVALUATOR( pow )
HIVE_DEFINE_EVALUATOR( pow2 )
HIVE_DEFINE_EVALUATOR( feed_publish )
HIVE_DEFINE_EVALUATOR( witness_block_approve )

void register_witness_evaluators( evaluator_registry<operation>& registry )
{
  registry.register_evaluator< witness_update_evaluator         >();
  registry.register_evaluator< witness_set_properties_evaluator >();
  registry.register_evaluator< account_witness_proxy_evaluator  >();
  registry.register_evaluator< account_witness_vote_evaluator   >();
  registry.register_evaluator< custom_evaluator                 >();
  registry.register_evaluator< custom_json_evaluator            >();
  registry.register_evaluator< custom_binary_evaluator          >();
  registry.register_evaluator< pow_evaluator                    >();
  registry.register_evaluator< pow2_evaluator                   >();
  registry.register_evaluator< feed_publish_evaluator           >();
  registry.register_evaluator< witness_block_approve_evaluator  >();
}

void copy_legacy_chain_properties( chain_properties& dest, const legacy_chain_properties& src )
{
  dest.account_creation_fee = src.get_account_creation_fee();
  dest.maximum_block_size = src.maximum_block_size;
  dest.hbd_interest_rate = src.hbd_interest_rate;
}

void witness_update_evaluator::do_apply( const witness_update_operation& o )
{
  _db.get_account( o.owner ); // verify owner exists

  if ( _db.has_hardfork( HIVE_HARDFORK_0_14__410 ) )
  {
    FC_ASSERT( o.props.account_creation_fee.symbol.is_canon() );
  }
  else if( !o.props.account_creation_fee.symbol.is_canon() )
  {
    // there are 8 instances of non-canon fee symbol in mainnet, so the check can't be in validate()
    push_virtual_operation( _db, system_warning_operation( FC_LOG_MESSAGE( warn,
      "Wrong fee symbol in block ${b}", ( "b", _db.head_block_num() + 1 ) ).get_message() ) );
  }

  const auto& by_witness_name_idx = _db.get_index< witness_index >().indices().get< by_name >();
  auto wit_itr = by_witness_name_idx.find( o.owner );
  if( wit_itr != by_witness_name_idx.end() )
  {
    _db.modify( *wit_itr, [&]( witness_object& w )
    {
      from_string( w.url, o.url );
      w.signing_key = o.block_signing_key;
      copy_legacy_chain_properties( w.props, o.props );
    } );
  }
  else
  {
    _db.create< witness_object >( [&]( witness_object& w )
    {
      w.owner = o.owner;
      from_string( w.url, o.url );
      w.signing_key = o.block_signing_key;
      w.created = _db.head_block_time();
      copy_legacy_chain_properties( w.props, o.props );
    } );
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
    asset fee;
    fc::raw::unpack_from_vector( itr->second, fee );
    props.account_creation_fee = fee;
  }

  itr = o.props.find( "maximum_block_size" );
  flags.max_block_changed = itr != o.props.end();
  if( flags.max_block_changed )
    fc::raw::unpack_from_vector( itr->second, props.maximum_block_size );

  itr = o.props.find( "sbd_interest_rate" );
  if(itr == o.props.end() && _db.has_hardfork(HIVE_HARDFORK_1_24))
    itr = o.props.find( "hbd_interest_rate" );
  flags.hbd_interest_changed = itr != o.props.end();
  if( flags.hbd_interest_changed )
    fc::raw::unpack_from_vector( itr->second, props.hbd_interest_rate );

  itr = o.props.find( "account_subsidy_budget" );
  flags.account_subsidy_budget_changed = itr != o.props.end();
  if( flags.account_subsidy_budget_changed )
    fc::raw::unpack_from_vector( itr->second, props.account_subsidy_budget );

  itr = o.props.find( "account_subsidy_decay" );
  flags.account_subsidy_decay_changed = itr != o.props.end();
  if( flags.account_subsidy_decay_changed )
    fc::raw::unpack_from_vector( itr->second, props.account_subsidy_decay );

  itr = o.props.find( "new_signing_key" );
  flags.key_changed = itr != o.props.end();
  if( flags.key_changed )
    fc::raw::unpack_from_vector( itr->second, signing_key );

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
    fc::raw::unpack_from_vector< std::string >( itr->second, url );

  _db.modify( witness, [&]( witness_object& w )
  {
    if( flags.account_creation_changed )
      w.props.account_creation_fee = props.account_creation_fee;
    if( flags.max_block_changed )
      w.props.maximum_block_size = props.maximum_block_size;
    if( flags.hbd_interest_changed )
      w.props.hbd_interest_rate = props.hbd_interest_rate;
    if( flags.account_subsidy_budget_changed )
      w.props.account_subsidy_budget = props.account_subsidy_budget;
    if( flags.account_subsidy_decay_changed )
      w.props.account_subsidy_decay = props.account_subsidy_decay;
    if( flags.key_changed )
      w.signing_key = signing_key;
    if( flags.hbd_exchange_changed )
    {
      w.hbd_exchange_rate = hbd_exchange_rate;
      w.last_hbd_exchange_update = last_hbd_exchange_update;
    }
    if( flags.url_changed )
      from_string( w.url, url );
  } );
}

void account_witness_proxy_evaluator::do_apply( const account_witness_proxy_operation& o )
{
  const auto& account = _db.get_account( o.account );
  FC_ASSERT( account.can_vote && "Account has declined the ability to vote and cannot proxy votes." );
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
        push_virtual_operation( _db, proxy_cleared_operation( account.get_name(), _db.get_account( account.get_proxy() ).get_name() ) );
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

    push_virtual_operation( _db, proxy_cleared_operation( account.get_name(), _db.get_account( account.get_proxy() ).get_name() ) );

    _db.modify( account, [&]( account_object& a ) {
      a.clear_proxy();
    });
  }
}


void account_witness_vote_evaluator::do_apply( const account_witness_vote_operation& o )
{
  const auto& voter = _db.get_account( o.account );
  FC_ASSERT( !voter.has_proxy(), "A proxy is currently set, please clear the proxy before voting for a witness." );
  FC_ASSERT( voter.can_vote && "Account has declined its voting rights." );
  _db.modify( voter, [&]( account_object& a) { a.update_governance_vote_expiration_ts(_db.head_block_time()); });

  const auto& witness = _db.get_witness( o.witness );

  const auto& by_account_witness_idx = _db.get_index< witness_vote_index >().indices().get< by_account_witness >();
  auto itr = by_account_witness_idx.find( boost::make_tuple( voter.get_name(), witness.owner ) );

  if( itr == by_account_witness_idx.end() )
  {
    FC_ASSERT( o.approve, "Vote doesn't exist, user must indicate a desire to approve witness." );

    if( _db.has_hardfork( HIVE_HARDFORK_0_2 ) ) // a49e13be78bf337c95967418d6ead76565515385 pushed fminerten above limit
      FC_ASSERT( voter.witnesses_voted_for < HIVE_MAX_ACCOUNT_WITNESS_VOTES, "Account has voted for too many witnesses." );

    _db.create<witness_vote_object>( [&]( witness_vote_object& v )
    {
      v.witness = witness.owner;
      v.account = voter.get_name();
    } );

    if( _db.has_hardfork( HIVE_HARDFORK_0_3 ) )
      _db.adjust_witness_vote( witness, voter.get_governance_vote_power() );
    else if( _db.has_hardfork( HIVE_HARDFORK_0_2 ) )
      _db.adjust_proxied_witness_votes( voter, voter.get_governance_vote_power() );
    else
    {
      _db.modify( witness, [&]( witness_object& w )
      {
        w.votes += voter.get_governance_vote_power();
      } );
    }
    _db.modify( voter, [&]( account_object& a )
    {
      a.witnesses_voted_for++;
    } );
  }
  else
  {
    FC_ASSERT( !o.approve, "Vote currently exists, user must indicate a desire to reject witness." );

    if( _db.has_hardfork( HIVE_HARDFORK_0_3 ) )
      _db.adjust_witness_vote( witness, -voter.get_governance_vote_power() );
    else if( _db.has_hardfork( HIVE_HARDFORK_0_2 ) )
      _db.adjust_proxied_witness_votes( voter, -voter.get_governance_vote_power() );
    else
    {
      _db.modify( witness, [&]( witness_object& w )
      {
        w.votes -= voter.get_governance_vote_power();
      } );
    }
    _db.modify( voter, [&]( account_object& a )
    {
      a.witnesses_voted_for--;
    } );
    _db.remove( *itr );
  }
}

void custom_evaluator::do_apply( const custom_operation& o )
{
  if( _db.has_hardfork( HIVE_HARDFORK_1_26_SOLIDIFY_OLD_SOFTFORKS ) ) // ab28c8e3a10d24f56476653d6a525e712e2e912e example tx with big op
  {
    FC_ASSERT( o.data.size() <= HIVE_CUSTOM_OP_DATA_MAX_LENGTH,
      "Operation data must be less than ${bytes} bytes.", ("bytes", HIVE_CUSTOM_OP_DATA_MAX_LENGTH) );
  }
}

void custom_json_evaluator::do_apply( const custom_json_operation& o )
{
  using hive::protocol::details::truncation_controller;

  if( _db.has_hardfork( HIVE_HARDFORK_1_26_SOLIDIFY_OLD_SOFTFORKS ) ) // 803bcc0dae4d242e0a6539948d998a4410b19655 example tx of big json
  {
    FC_ASSERT( o.json.length() <= HIVE_CUSTOM_OP_DATA_MAX_LENGTH,
      "Operation JSON must be less than ${bytes} bytes.", ("bytes", HIVE_CUSTOM_OP_DATA_MAX_LENGTH) );
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
  FC_ASSERT( false && "custom_binary_operation is disallowed" ); //ABW: since no one used it in practice
    //it waits for potential redesign until it is reenabled
  FC_ASSERT( _db.has_hardfork( HIVE_HARDFORK_0_14__317 ) );

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
      FC_ASSERT( !"DUPLICATE WORK DISCOVERED", "${w}  ${witness}",("w",o)("wit",*work_itr) );
  }

  const auto& accounts_by_name = db.get_index<account_index>().indices().get<by_name>();

  auto itr = accounts_by_name.find( o.worker_account );
  if(itr == accounts_by_name.end())
  {
    const auto& new_account = create_account( db, o.worker_account, o.work.worker, dgp.time, db.get_current_timestamp(),
      true /*mined*/, HIVE_asset( 0 ) );
    // ^ empty recovery account parameter means highest voted witness at time of recovery

#ifdef COLLECT_ACCOUNT_METADATA
    db.create< account_metadata_object >( [&]( account_metadata_object& meta )
    {
      meta.account = new_account.get_id();
    } );
#else
    FC_UNUSED( new_account );
#endif

    db.create< account_authority_object >( [&]( account_authority_object& auth )
    {
      auth.account = o.worker_account;
      auth.owner = authority( 1, o.work.worker, 1 );
      auth.active = auth.owner;
      auth.posting = auth.owner;
    } );

    push_virtual_operation( db, account_created_operation( new_account.get_name(), o.worker_account, VEST_asset( 0 ), VEST_asset( 0 ) ) );
  }

  const auto& worker_account = db.get_account( o.worker_account ); // verify it exists
#ifndef HIVE_CONVERTER_BUILD // disable these checks, since there is a 2nd auth applied on all the accs in the alternate chain generated using hive blockchain converter
  const auto& worker_auth = db.get< account_authority_object, by_account >( o.worker_account );
  FC_ASSERT( worker_auth.active.num_auths() == 1, "Miners can only have one key authority. ${a}", ("a",worker_auth.active) );
  FC_ASSERT( worker_auth.active.key_auths.size() == 1, "Miners may only have one key authority." );
  FC_ASSERT( worker_auth.active.key_auths.begin()->first == o.work.worker, "Work must be performed by key that signed the work." );
#endif
  FC_ASSERT( o.block_id == db.head_block_id(), "pow not for last block" );
  // there used to be limit preventing pow operation following worker account update in the same block (since HF13)

#ifndef HIVE_CONVERTER_BUILD // due to the optimization issues with blockchain_converter performing proof of work for every pow operations, this check is applied only in mainnet
  fc::sha256 target = db.get_pow_target();

  FC_ASSERT( o.work.work < target, "Work lacks sufficient difficulty." );
#endif

  db.modify( dgp, [&]( dynamic_global_property_object& p )
  {
    p.total_pow++; // make sure this doesn't break anything...
    p.num_pow_witnesses++;
  } );


  const witness_object* cur_witness = db.find_witness( worker_account.get_name() );
  if( cur_witness )
  {
    FC_ASSERT( cur_witness->pow_worker == 0, "This account is already scheduled for pow block production." );
    db.modify(*cur_witness, [&]( witness_object& w )
    {
      copy_legacy_chain_properties( w.props, o.props );
      w.pow_worker = dgp.total_pow;
      w.last_work = o.work.work;
    } );
  }
  else
  {
    db.create<witness_object>( [&]( witness_object& w )
    {
      w.owner = o.worker_account;
      copy_legacy_chain_properties( w.props, o.props );
      w.created = db.head_block_time();
      w.signing_key = o.work.worker;
      w.pow_worker = dgp.total_pow;
      w.last_work = o.work.work;
    } );
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
  push_virtual_operation( db, pow_reward_operation( dgp.current_witness, actual_reward ) );
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
//    auto recent_block_num = protocol::block_header::num_from_id( work.input.prev_block );
//    FC_ASSERT( recent_block_num >= db.get_last_irreversible_block_num(),
//      "Equihash pow done for block older than last irreversible block num. pow block: ${recent_block_num}, last irreversible block: ${lib}", (recent_block_num)("lib", db.get_last_irreversible_block_num()));
// ABW: above check was not really valid because LIB is not strict part of consensus; we can remove it
//      safely because there are no new pow operations (and it conflicts with "undo-less" checkpoints)
#ifndef HIVE_CONVERTER_BUILD
    FC_ASSERT( work.pow_summary < target_pow && "Post HF16", "Insufficient work difficulty. Work: ${w}, Target: ${t}", ("w",work.pow_summary)("t", target_pow) );
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
    const auto& new_account = create_account( db, worker_account, *o.new_owner_key, dgp.time, _db.get_current_timestamp(),
      true /*mined*/, HIVE_asset( 0 ) );
    // ^ empty recovery account parameter means highest voted witness at time of recovery

#ifdef COLLECT_ACCOUNT_METADATA
    db.create< account_metadata_object >( [&]( account_metadata_object& meta )
    {
      meta.account = new_account.get_id();
    } );
#else
    FC_UNUSED( new_account );
#endif

    db.create< account_authority_object >( [&]( account_authority_object& auth )
    {
      auth.account = worker_account;
      auth.owner = authority( 1, *o.new_owner_key, 1 );
      auth.active = auth.owner;
      auth.posting = auth.owner;
    } );

    db.create<witness_object>( [&]( witness_object& w )
    {
      w.owner = worker_account;
      copy_legacy_chain_properties( w.props, o.props );
      w.created = db.head_block_time();
      w.signing_key = *o.new_owner_key;
      w.pow_worker = dgp.total_pow;
    } );

    push_virtual_operation( _db, account_created_operation( new_account.get_name(), worker_account, VEST_asset( 0 ), VEST_asset( 0 ) ) );
  }
  else
  {
    FC_ASSERT( !o.new_owner_key.valid(), "Cannot specify an owner key unless creating account." );
    const witness_object* cur_witness = db.find_witness( worker_account );
    FC_ASSERT( cur_witness, "Witness must be created for existing account before mining.");
    FC_ASSERT( cur_witness->pow_worker == 0 && "This account is already scheduled for pow block production." );
    db.modify(*cur_witness, [&]( witness_object& w )
    {
      copy_legacy_chain_properties( w.props, o.props );
      w.pow_worker = dgp.total_pow;
    } );
  }

  if( !db.has_hardfork( HIVE_HARDFORK_0_16__551) )
  {
    /// pay the witness that includes this POW
    asset inc_reward = db.get_pow_reward();
    db.adjust_supply( inc_reward, true );

    const auto& inc_witness = db.get_account( dgp.current_witness );
    asset actual_reward = db.create_vesting( inc_witness, inc_reward );
    push_virtual_operation( db, pow_reward_operation( dgp.current_witness, actual_reward ) );
  }
}

void feed_publish_evaluator::do_apply( const feed_publish_operation& o )
{
  if( _db.has_hardfork( HIVE_HARDFORK_0_20__409 ) ) // f87cc43e41e33d7d125f6f63bb9d2dcfc900ba9b - example of reverted feed
  {
    // ABW: unfortunately existence of reversed feed means we won't be able to use "tiny price" for feeds;
    // we can't normalize it either, because it would change price sorting, therefore also median price
    FC_ASSERT( is_asset_type( o.exchange_rate.base, HBD_SYMBOL ) && is_asset_type( o.exchange_rate.quote, HIVE_SYMBOL ),
      "Price feed must be a HBD/HIVE price" );
  }

  const auto& witness = _db.get_witness( o.publisher );
  _db.modify( witness, [&]( witness_object& w )
  {
    w.hbd_exchange_rate = o.exchange_rate;
    w.last_hbd_exchange_update = _db.head_block_time();
  });
}

void witness_block_approve_evaluator::do_apply(const witness_block_approve_operation& op)
{
  // This transaction si /updait's handled in database::process_fast_confirm_transaction
  // and never reaches the
  FC_ASSERT( false && "This operation may not be included in a block");
}

} } // hive::chain
