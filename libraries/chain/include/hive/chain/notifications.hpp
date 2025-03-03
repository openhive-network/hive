#pragma once

#include <hive/protocol/block.hpp>
#include <hive/chain/full_block.hpp>
#include <hive/chain/full_transaction.hpp>

namespace hive { namespace chain {

struct block_notification
{
  block_notification( const std::shared_ptr<full_block_type>& full_block ) :
    block_id(full_block->get_block_id()),
    prev_block_id(full_block->get_block_header().previous),
    block_num(full_block->get_block_num()),
    full_block(full_block)
  {
  }

  fc::time_point_sec get_block_timestamp() const { return full_block->get_block_header().timestamp; }

  hive::protocol::block_id_type           block_id;
  hive::protocol::block_id_type           prev_block_id;
  uint32_t                                block_num = 0;
  const std::shared_ptr<full_block_type>& full_block;
};

struct transaction_notification
{
  transaction_notification( const std::shared_ptr<full_transaction_type>& full_transaction ) :
    transaction_id(full_transaction->get_transaction_id()),
    transaction(full_transaction->get_transaction()),
    full_transaction(full_transaction)
  {}

  hive::protocol::transaction_id_type           transaction_id;
  const hive::protocol::signed_transaction&     transaction;
  const std::shared_ptr<full_transaction_type>& full_transaction;
};

struct operation_notification
{
  operation_notification( const hive::protocol::operation& o ) : op(o) {}

  transaction_id_type trx_id;
  int64_t             block = 0;
  fc::time_point_sec  timestamp;
  int64_t             trx_in_block = 0;
  int64_t             op_in_trx = 0;
  const hive::protocol::operation&    op;
  bool                virtual_op = false;
};

struct comment_reward_notification
{
  comment_reward_notification( const share_type& _total_reward, share_type _author_tokens, share_type _curation_tokens )
  : total_reward( _total_reward ), author_tokens( _author_tokens ), curation_tokens( _curation_tokens ) {}

  share_type total_reward;
  share_type author_tokens;
  share_type curation_tokens;
};

struct remove_comment_cashout_notification
{
  remove_comment_cashout_notification( const comment_id_type& _comment_id, const account_id_type& _account_id, const std::string& _permlink )
  : comment_id( _comment_id ), account_id( _account_id ), permlink( _permlink ) {}

  comment_id_type comment_id;
  account_id_type account_id;
  std::string     permlink;
};

} }
