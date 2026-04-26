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

    const auto& exchange_rate = _db.get_hbd_price();
    const auto& rf = _db.get_reward_fund();

    u256 total_r2 = chain::util::to256( rf.get_recent_claims() );

    if( total_r2 > 0 )
    {
      HIVE_asset reward_hive = rf.get_reward_balance();
      uint128_t vshares = chain::util::evaluate_reward_curve( cashout->get_net_rshares(), rf.get_author_reward_curve(), rf.get_content_constant() );

      HBD_asset reward_hbd( 0 );
      if( !exchange_rate.is_null() )
        reward_hbd = reward_hive * exchange_rate;

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
