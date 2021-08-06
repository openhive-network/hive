#pragma once

#include <hive/chain/hive_fwd.hpp>

#include <chainbase/chainbase.hpp>
#include <chainbase/util/object_id_serialization.hpp>

#include <hive/protocol/types.hpp>
#include <hive/protocol/authority.hpp>

#include <hive/chain/buffer_type.hpp>

#include <hive/chain/multi_index_types.hpp>

namespace hive {

namespace protocol {

struct asset;
struct price;

}

namespace chain {

using chainbase::object;
using chainbase::oid;
using chainbase::oid_ref;
using chainbase::allocator;

using hive::protocol::block_id_type;
using hive::protocol::transaction_id_type;
using hive::protocol::chain_id_type;
using hive::protocol::account_name_type;
using hive::protocol::share_type;
using hive::protocol::ushare_type;

using chainbase::shared_string;

inline std::string to_string( const shared_string& str ) { return std::string( str.begin(), str.end() ); }
inline void from_string( shared_string& out, const string& in ){ out.assign( in.begin(), in.end() ); }

using chainbase::by_id;
struct by_name;

enum object_type
{
  dynamic_global_property_object_type,
  account_object_type,
  account_metadata_object_type,
  account_authority_object_type,
  witness_object_type,
  transaction_object_type,
  block_summary_object_type,
  witness_schedule_object_type,
  comment_object_type,
  comment_vote_object_type,
  witness_vote_object_type,
  limit_order_object_type,
  feed_history_object_type,
  convert_request_object_type,
  collateralized_convert_request_object_type,
  liquidity_reward_balance_object_type,
  operation_object_type,
  account_history_object_type,
  hardfork_property_object_type,
  withdraw_vesting_route_object_type,
  owner_authority_history_object_type,
  account_recovery_request_object_type,
  change_recovery_account_request_object_type,
  escrow_object_type,
  savings_withdraw_object_type,
  decline_voting_rights_request_object_type,
  block_stats_object_type,
  reward_fund_object_type,
  vesting_delegation_object_type,
  vesting_delegation_expiration_object_type,
  pending_required_action_object_type,
  pending_optional_action_object_type,
  proposal_object_type,
  proposal_vote_object_type,
  comment_cashout_object_type,
  comment_cashout_ex_object_type,
  recurrent_transfer_object_type,
#ifdef HIVE_ENABLE_SMT
  // SMT objects
  smt_token_object_type,
  account_regular_balance_object_type,
  account_rewards_balance_object_type,
  nai_pool_object_type,
  smt_token_emissions_object_type,
  smt_contribution_object_type,
  smt_ico_object_type,
#endif
};

class dynamic_global_property_object;
class account_object;
class account_metadata_object;
class account_authority_object;
class witness_object;
class transaction_object;
class block_summary_object;
class witness_schedule_object;
class comment_object;
class comment_vote_object;
class witness_vote_object;
class limit_order_object;
class feed_history_object;
class convert_request_object;
class collateralized_convert_request_object;
class liquidity_reward_balance_object;
class operation_object;
class account_history_object;
class hardfork_property_object;
class withdraw_vesting_route_object;
class owner_authority_history_object;
class account_recovery_request_object;
class change_recovery_account_request_object;
class escrow_object;
class savings_withdraw_object;
class decline_voting_rights_request_object;
class block_stats_object;
class reward_fund_object;
class vesting_delegation_object;
class vesting_delegation_expiration_object;
class pending_required_action_object;
class pending_optional_action_object;
class comment_cashout_object;
class comment_cashout_ex_object;
class recurrent_transfer_object;

#ifdef HIVE_ENABLE_SMT
class smt_token_object;
class account_regular_balance_object;
class account_rewards_balance_object;
class nai_pool_object;
class smt_token_emissions_object;
class smt_contribution_object;
class smt_ico_object;
#endif

class proposal_object;
class proposal_vote_object;

typedef oid_ref< dynamic_global_property_object         > dynamic_global_property_id_type;
typedef oid_ref< account_object                         > account_id_type;
typedef oid_ref< account_metadata_object                > account_metadata_id_type;
typedef oid_ref< account_authority_object               > account_authority_id_type;
typedef oid_ref< witness_object                         > witness_id_type;
typedef oid_ref< transaction_object                     > transaction_object_id_type;
typedef oid_ref< block_summary_object                   > block_summary_id_type;
typedef oid_ref< witness_schedule_object                > witness_schedule_id_type;
typedef oid_ref< comment_object                         > comment_id_type;
typedef oid_ref< comment_vote_object                    > comment_vote_id_type;
typedef oid_ref< witness_vote_object                    > witness_vote_id_type;
typedef oid_ref< limit_order_object                     > limit_order_id_type;
typedef oid_ref< feed_history_object                    > feed_history_id_type;
typedef oid_ref< convert_request_object                 > convert_request_id_type;
typedef oid_ref< collateralized_convert_request_object  > collateralized_convert_request_id_type;
typedef oid_ref< liquidity_reward_balance_object        > liquidity_reward_balance_id_type;
typedef oid_ref< operation_object                       > operation_id_type;
typedef oid_ref< account_history_object                 > account_history_id_type;
typedef oid_ref< hardfork_property_object               > hardfork_property_id_type;
typedef oid_ref< withdraw_vesting_route_object          > withdraw_vesting_route_id_type;
typedef oid_ref< owner_authority_history_object         > owner_authority_history_id_type;
typedef oid_ref< account_recovery_request_object        > account_recovery_request_id_type;
typedef oid_ref< change_recovery_account_request_object > change_recovery_account_request_id_type;
typedef oid_ref< escrow_object                          > escrow_id_type;
typedef oid_ref< savings_withdraw_object                > savings_withdraw_id_type;
typedef oid_ref< decline_voting_rights_request_object   > decline_voting_rights_request_id_type;
typedef oid_ref< block_stats_object                     > block_stats_id_type;
typedef oid_ref< reward_fund_object                     > reward_fund_id_type;
typedef oid_ref< vesting_delegation_object              > vesting_delegation_id_type;
typedef oid_ref< vesting_delegation_expiration_object   > vesting_delegation_expiration_id_type;
typedef oid_ref< pending_required_action_object         > pending_required_action_id_type;
typedef oid_ref< pending_optional_action_object         > pending_optional_action_id_type;
typedef oid_ref< comment_cashout_object                 > comment_cashout_id_type;
typedef oid_ref< comment_cashout_ex_object              > comment_cashout_ex_id_type;
typedef oid_ref< recurrent_transfer_object              > recurrent_transfer_id_type;

#ifdef HIVE_ENABLE_SMT
typedef oid_ref< smt_token_object                       > smt_token_id_type;
typedef oid_ref< account_regular_balance_object         > account_regular_balance_id_type;
typedef oid_ref< account_rewards_balance_object         > account_rewards_balance_id_type;
typedef oid_ref< nai_pool_object                        > nai_pool_id_type;
typedef oid_ref< smt_token_emissions_object             > smt_token_emissions_id_type;
typedef oid_ref< smt_contribution_object                > smt_contribution_id_type;
typedef oid_ref< smt_ico_object                         > smt_ico_id_type;
#endif

typedef oid_ref< proposal_object                        > proposal_id_type;
typedef oid_ref< proposal_vote_object                   > proposal_vote_id_type;

enum bandwidth_type
{
  post,    ///< Rate limiting posting reward eligibility over time
  forum,   ///< Rate limiting for all forum related actions
  market   ///< Rate limiting for all other actions
};

} } //hive::chain

namespace fc
{
class variant;

#ifndef ENABLE_STD_ALLOCATOR
inline void to_variant( const hive::chain::shared_string& s, variant& var )
{
  var = fc::string( hive::chain::to_string( s ) );
}

inline void from_variant( const variant& var, hive::chain::shared_string& s )
{
  auto str = var.as_string();
  s.assign( str.begin(), str.end() );
}
#endif

namespace raw
{

#ifndef ENABLE_STD_ALLOCATOR
template< typename Stream >
void pack( Stream& s, const chainbase::shared_string& ss )
{
  std::string str = hive::chain::to_string( ss );
  fc::raw::pack( s, str );
}

template< typename Stream >
void unpack( Stream& s, chainbase::shared_string& ss, uint32_t depth )
{
  depth++;
  std::string str;
  fc::raw::unpack( s, str, depth );
  hive::chain::from_string( ss, str );
}
#endif

template< typename Stream, typename E, typename A >
void pack( Stream& s, const boost::interprocess::deque<E, A>& dq )
{
  // This could be optimized
  std::vector<E> temp;
  std::copy( dq.begin(), dq.end(), std::back_inserter(temp) );
  pack( s, temp );
}

template< typename Stream, typename E, typename A >
void unpack( Stream& s, boost::interprocess::deque<E, A>& dq, uint32_t depth )
{
  depth++;
  FC_ASSERT( depth <= MAX_RECURSION_DEPTH );
  // This could be optimized
  std::vector<E> temp;
  unpack( s, temp, depth );
  dq.clear();
  std::copy( temp.begin(), temp.end(), std::back_inserter(dq) );
}

template< typename Stream, typename K, typename V, typename C, typename A >
void pack( Stream& s, const boost::interprocess::flat_map< K, V, C, A >& value )
{
  fc::raw::pack( s, unsigned_int((uint32_t)value.size()) );
  auto itr = value.begin();
  auto end = value.end();
  while( itr != end )
  {
    fc::raw::pack( s, *itr );
    ++itr;
  }
}

template< typename Stream, typename K, typename V, typename C, typename A >
void unpack( Stream& s, boost::interprocess::flat_map< K, V, C, A >& value, uint32_t depth )
{
  depth++;
  FC_ASSERT( depth <= MAX_RECURSION_DEPTH );
  unsigned_int size;
  unpack( s, size, depth );
  value.clear();
  FC_ASSERT( size.value*(sizeof(K)+sizeof(V)) < MAX_ARRAY_ALLOC_SIZE );
  for( uint32_t i = 0; i < size.value; ++i )
  {
    std::pair<K,V> tmp;
    fc::raw::unpack( s, tmp, depth );
    value.insert( std::move(tmp) );
  }
}

#ifndef ENABLE_STD_ALLOCATOR
template< typename T >
T unpack_from_vector( const hive::chain::buffer_type& s )
{
  try
  {
    T tmp;
    if( s.size() )
    {
      datastream<const char*>  ds( s.data(), size_t(s.size()) );
      fc::raw::unpack(ds,tmp);
    }
    return tmp;
  } FC_RETHROW_EXCEPTIONS( warn, "error unpacking ${type}", ("type",fc::get_typename<T>::name() ) )
}
#endif
} } // namespace fc::raw

FC_REFLECT_ENUM( hive::chain::object_type,
            (dynamic_global_property_object_type)
            (account_object_type)
            (account_metadata_object_type)
            (account_authority_object_type)
            (witness_object_type)
            (transaction_object_type)
            (block_summary_object_type)
            (witness_schedule_object_type)
            (comment_object_type)
            (comment_vote_object_type)
            (witness_vote_object_type)
            (limit_order_object_type)
            (feed_history_object_type)
            (convert_request_object_type)
            (collateralized_convert_request_object_type)
            (liquidity_reward_balance_object_type)
            (operation_object_type)
            (account_history_object_type)
            (hardfork_property_object_type)
            (withdraw_vesting_route_object_type)
            (owner_authority_history_object_type)
            (account_recovery_request_object_type)
            (change_recovery_account_request_object_type)
            (escrow_object_type)
            (savings_withdraw_object_type)
            (decline_voting_rights_request_object_type)
            (block_stats_object_type)
            (reward_fund_object_type)
            (vesting_delegation_object_type)
            (vesting_delegation_expiration_object_type)
            (pending_required_action_object_type)
            (pending_optional_action_object_type)
            (proposal_object_type)
            (proposal_vote_object_type)
            (comment_cashout_object_type)
            (comment_cashout_ex_object_type)
            (recurrent_transfer_object_type)

#ifdef HIVE_ENABLE_SMT
            (smt_token_object_type)
            (account_regular_balance_object_type)
            (account_rewards_balance_object_type)
            (nai_pool_object_type)
            (smt_token_emissions_object_type)
            (smt_contribution_object_type)
            (smt_ico_object_type)
#endif
          )

#ifndef ENABLE_STD_ALLOCATOR
FC_REFLECT_TYPENAME( hive::chain::shared_string )
#endif

FC_REFLECT_ENUM( hive::chain::bandwidth_type, (post)(forum)(market) )
