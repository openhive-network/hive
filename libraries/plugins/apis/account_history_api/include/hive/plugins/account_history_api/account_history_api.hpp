#pragma once
#include <hive/plugins/json_rpc/utility.hpp>

#include <hive/chain/history_object.hpp>

#include <hive/protocol/types.hpp>

#include <fc/optional.hpp>
#include <fc/variant.hpp>
#include <fc/vector.hpp>

namespace hive { namespace plugins { namespace account_history {


namespace detail { class abstract_account_history_api_impl; }

struct api_operation_object
{
  api_operation_object() {}

  template< typename T >
  api_operation_object( const T& op_obj ) :
    trx_id( op_obj.trx_id ),
    block( op_obj.block ),
    trx_in_block( op_obj.trx_in_block ),
    virtual_op( op_obj.virtual_op ),
    timestamp( op_obj.timestamp )
  {
    op = fc::raw::unpack_from_buffer< hive::protocol::operation >( op_obj.serialized_op );
  }

  hive::protocol::transaction_id_type trx_id;
  uint32_t                            block = 0;
  uint32_t                            trx_in_block = 0;
  uint32_t                            op_in_trx = 0;
  uint32_t                            virtual_op = 0;
  uint64_t                            operation_id = 0;
  fc::time_point_sec                  timestamp;
  hive::protocol::operation           op;

  bool operator<( const api_operation_object& obj ) const
  {
    return std::tie( block, trx_in_block, op_in_trx, virtual_op ) < std::tie( obj.block, obj.trx_in_block, obj.op_in_trx, obj.virtual_op );
  }
};

template<template<typename T> typename optional_type>
struct get_ops_in_block_args_base
{
  uint32_t block_num = 0;
  bool     only_virtual = false;
  /// if set to true also operations from reversible block will be included if block_num points to such block.
  optional_type<bool> include_reversible;
};

using get_ops_in_block_args_signature = get_ops_in_block_args_base<fc::optional_init>;
using get_ops_in_block_args = get_ops_in_block_args_base<fc::optional>;

struct get_ops_in_block_return
{
  std::multiset< api_operation_object > ops;
};

template<template<typename T> typename optional_type>
struct get_transaction_args_base
{
  hive::protocol::transaction_id_type id;
  /// if set to true transaction from reversible block will be returned if id matches given one.
  optional_type<bool> include_reversible;
};

using get_transaction_args_signature = get_transaction_args_base<fc::optional_init>;
using get_transaction_args = get_transaction_args_base<fc::optional>;

typedef hive::protocol::annotated_signed_transaction get_transaction_return;

template<template<typename T> typename optional_type>
struct get_account_history_args_base
{
  hive::protocol::account_name_type   account;
  uint64_t                            start = -1;
  uint32_t                            limit = 1000;
  /// if set to true operations from reversible block will be also returned.
  optional_type<bool> include_reversible;
  /** if either are set, the set of returned operations will include only these 
   * matching bitwise filter.
   * For the first 64 operations (as defined in protocol/operations.hpp), set the 
   * corresponding bit in operation_filter_low; for the higher-numbered operations,
   * set the bit in operation_filter_high (pretending operation_filter is a 
   * 128-bit bitmask composed of {operation_filter_high, operation_filter_low})
   */
  optional_type<uint64_t> operation_filter_low;
  optional_type<uint64_t> operation_filter_high;
};

using get_account_history_args_signature  = get_account_history_args_base<fc::optional_init>;
using get_account_history_args            = get_account_history_args_base<fc::optional>;

struct get_account_history_return
{
  std::map< uint32_t, api_operation_object > history;
};

enum enum_vops_filter : uint64_t
{
  fill_convert_request_operation                = 0x0'00000001ull,
  author_reward_operation                       = 0x0'00000002ull,
  curation_reward_operation                     = 0x0'00000004ull,
  comment_reward_operation                      = 0x0'00000008ull,
  liquidity_reward_operation                    = 0x0'00000010ull,
  interest_operation                            = 0x0'00000020ull,
  fill_vesting_withdraw_operation               = 0x0'00000040ull,
  fill_order_operation                          = 0x0'00000080ull,
  shutdown_witness_operation                    = 0x0'00000100ull,
  fill_transfer_from_savings_operation          = 0x0'00000200ull,
  hardfork_operation                            = 0x0'00000400ull,
  comment_payout_update_operation               = 0x0'00000800ull,
  return_vesting_delegation_operation           = 0x0'00001000ull,
  comment_benefactor_reward_operation           = 0x0'00002000ull,
  producer_reward_operation                     = 0x0'00004000ull,
  clear_null_account_balance_operation          = 0x0'00008000ull,
  proposal_pay_operation                        = 0x0'00010000ull,
  sps_fund_operation                            = 0x0'00020000ull,
  hardfork_hive_operation                       = 0x0'00040000ull,
  hardfork_hive_restore_operation               = 0x0'00080000ull,
  delayed_voting_operation                      = 0x0'00100000ull,
  consolidate_treasury_balance_operation        = 0x0'00200000ull,
  effective_comment_vote_operation              = 0x0'00400000ull,
  ineffective_delete_comment_operation          = 0x0'00800000ull,
  sps_convert_operation                         = 0x0'01000000ull,
  expired_account_notification_operation        = 0x0'02000000ull,
  changed_recovery_account_operation            = 0x0'04000000ull,
  transfer_to_vesting_completed_operation       = 0x0'08000000ull,
  pow_reward_operation                          = 0x0'10000000ull,
  vesting_shares_split_operation                = 0x0'20000000ull,
  account_created_operation                     = 0x0'40000000ull,
  fill_collateralized_convert_request_operation = 0x0'80000000ull,
  system_warning_operation                      = 0x1'00000000ull,
  fill_recurrent_transfer_operation             = 0x2'00000000ull,
  failed_recurrent_transfer_operation           = 0x4'00000000ull,
};

/** Allows to specify range of blocks to retrieve virtual operations for.
  *  \param block_range_begin - starting block number (inclusive) to search for virtual operations
  *  \param block_range_end   - last block number (exclusive) to search for virtual operations
  *  \param operation_begin   - starting virtual operation in given block (inclusive)
  *  \param limit             - a limit of retrieved operations
  *  \param filter            - a filter that decides which an operation matches - used bitwise filtering equals to position in `hive::protocol::operation`
  */
template<template<typename T> typename optional_type>
struct enum_virtual_ops_args_base
{
  uint32_t block_range_begin = 1;
  uint32_t block_range_end = 2;
  /// if set to true operations from reversible block will be also returned.
  optional_type<bool> include_reversible;

  optional_type<bool> group_by_block;
  optional_type< uint64_t > operation_begin;
  optional_type< uint32_t > limit;
  optional_type< uint64_t > filter;
};

using enum_virtual_ops_args_signature  = enum_virtual_ops_args_base<fc::optional_init>;
using enum_virtual_ops_args            = enum_virtual_ops_args_base<fc::optional>;

struct ops_array_wrapper
{
  ops_array_wrapper(uint32_t _block) : block(_block) {}

  uint32_t                          block = 0;
  bool                              irreversible = false;
  fc::time_point_sec                timestamp;
  std::vector<api_operation_object> ops;

  bool operator < (const ops_array_wrapper& rhs) const
    {
    return block < rhs.block;
    }
};

struct enum_virtual_ops_return
{
  vector<api_operation_object> ops;
  std::set<ops_array_wrapper>  ops_by_block;
  uint32_t                     next_block_range_begin = 0;
  uint64_t                     next_operation_begin   = 0;
};


class account_history_api
{
  public:
    account_history_api();
    ~account_history_api();

    DECLARE_API_SIGNATURE(
      (get_ops_in_block)
      (get_transaction)
      (get_account_history)
      (enum_virtual_ops)
    )

  private:
    std::unique_ptr< detail::abstract_account_history_api_impl > my;
};

} } } // hive::plugins::account_history

FC_REFLECT( hive::plugins::account_history::api_operation_object,
  (trx_id)(block)(trx_in_block)(op_in_trx)(virtual_op)(timestamp)(op)(operation_id) )

FC_REFLECT( hive::plugins::account_history::get_ops_in_block_args,
  (block_num)(only_virtual)(include_reversible) )

FC_REFLECT( hive::plugins::account_history::get_ops_in_block_args_signature,
  (block_num)(only_virtual)(include_reversible) )

FC_REFLECT( hive::plugins::account_history::get_ops_in_block_return,
  (ops) )

FC_REFLECT( hive::plugins::account_history::get_transaction_args,
  (id)(include_reversible) )

FC_REFLECT( hive::plugins::account_history::get_transaction_args_signature,
  (id)(include_reversible) )

FC_REFLECT( hive::plugins::account_history::get_account_history_args_signature,
  (account)(start)(limit)(include_reversible)(operation_filter_low)(operation_filter_high))

FC_REFLECT( hive::plugins::account_history::get_account_history_args,
  (account)(start)(limit)(include_reversible)(operation_filter_low)(operation_filter_high))

FC_REFLECT( hive::plugins::account_history::get_account_history_return,
  (history) )

FC_REFLECT( hive::plugins::account_history::enum_virtual_ops_args,
  (block_range_begin)(block_range_end)(include_reversible)(group_by_block)(operation_begin)(limit)(filter) )

FC_REFLECT( hive::plugins::account_history::enum_virtual_ops_args_signature,
  (block_range_begin)(block_range_end)(include_reversible)(group_by_block)(operation_begin)(limit)(filter) )

FC_REFLECT( hive::plugins::account_history::ops_array_wrapper, (block)(irreversible)(timestamp)(ops) )

FC_REFLECT( hive::plugins::account_history::enum_virtual_ops_return,
  (ops)(ops_by_block)(next_block_range_begin)(next_operation_begin) )
