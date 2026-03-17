#pragma once

#include <hive/protocol/asset.hpp>

#include <hive/chain/database.hpp>
#include <hive/chain/detail/state/comment_object.hpp>
#include <hive/chain/detail/state/reward_fund_object.hpp>
#include <hive/chain/detail/state/feed_history_object.hpp>
#include <hive/chain/util/reward.hpp>
#include <hive/chain/util/uint256.hpp>

namespace hive { namespace util {
namespace {
using hive::protocol::HIVE_asset;
using hive::protocol::HBD_asset;
using hive::protocol::effective_comment_vote_operation;

struct supplement_operations_visitor
{
  supplement_operations_visitor( const chain::database &db ) : _db(db) {}
  typedef void result_type;

  template <typename T>
  void operator()( const T &op ) {}

  void operator()( const effective_comment_vote_operation &op )
  {
    if( op.pending_payout.amount != 0 )
      return; //estimated payout was already calculated

    const auto* cashout = _db.find_comment_cashout( *_db.get_comment( op.author, op.permlink ) );
    if( cashout == nullptr || cashout->get_net_rshares() <= 0 )
      return; //voting can happen even after cashout; there will be no new payout though

    const auto& props = _db.get_dynamic_global_properties();
    const auto& hist = _db.get_feed_history();

    const chain::reward_fund_object* rf = nullptr;
    if( _db.has_hardfork( HIVE_HARDFORK_0_17__774 ) )
      rf = &_db.get_reward_fund();

    u256 total_r2 = 0;
    if( _db.has_hardfork( HIVE_HARDFORK_0_17__774 ) )
      total_r2 = chain::util::to256( rf->recent_claims );
    else
      total_r2 = chain::util::to256( props.get_total_reward_shares2() );

    if( total_r2 > 0 )
    {
      HIVE_asset reward_hive;
      uint128_t vshares = 0;

      if( _db.has_hardfork( HIVE_HARDFORK_0_17__774 ) )
      {
        reward_hive = rf->get_reward_balance();
        vshares = chain::util::evaluate_reward_curve( cashout->get_net_rshares(), rf->author_reward_curve, rf->content_constant );
      }
      else
      {
        reward_hive = props.get_total_reward_fund_hive();
        vshares = chain::util::evaluate_reward_curve( cashout->get_net_rshares() );
      }

      HBD_asset reward_hbd( 0 );
      if( !hist.current_median_history.is_null() )
        reward_hbd = reward_hive * hist.current_median_history;

      u256 r2 = chain::util::to256( vshares );
      r2 *= reward_hbd.get_amount();
      r2 /= total_r2;

      //ABW: yes, formally we should be making copy of the operation, however all things considered
      //it would be a waste of time
      effective_comment_vote_operation& _op = const_cast<effective_comment_vote_operation&>( op );
      _op.pending_payout.amount = static_cast<uint64_t>( r2 );
    }
  }

  const chain::database &_db;
};

} // namespace

inline void supplement_operation(const hive::protocol::operation &op, const hive::chain::database &db)
{
  supplement_operations_visitor vtor(db);
  op.visit(vtor);
}

} } // hive::util
