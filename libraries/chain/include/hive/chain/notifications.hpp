#pragma once

#include <hive/protocol/block.hpp>
#include <hive/chain/full_block.hpp>

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
  transaction_notification(const hive::protocol::signed_transaction& transaction, const hive::protocol::transaction_id_type& transaction_id) :
    transaction_id(transaction_id),
    transaction(transaction)
  {}

  hive::protocol::transaction_id_type          transaction_id;
  const hive::protocol::signed_transaction&    transaction;
};

struct operation_notification
{
  operation_notification( const hive::protocol::operation& o ) : op(o) {}

  transaction_id_type trx_id;
  int64_t             block = 0;
  int64_t             trx_in_block = 0;
  int64_t             op_in_trx = 0;
  const hive::protocol::operation&    op;
  bool                virtual_op = false;
};

struct required_action_notification
{
  required_action_notification( const hive::protocol::required_automated_action& a ) : action(a) {}

  const hive::protocol::required_automated_action& action;
};

struct optional_action_notification
{
  optional_action_notification( const hive::protocol::optional_automated_action& a ) : action(a) {}

  const hive::protocol::optional_automated_action& action;
};

struct comment_reward_notification
{
  comment_reward_notification( const share_type& _total_reward, share_type _author_tokens, share_type _curation_tokens )
  : total_reward( _total_reward ), author_tokens( _author_tokens ), curation_tokens( _curation_tokens ) {}

  share_type total_reward;
  share_type author_tokens;
  share_type curation_tokens;
};

} }
