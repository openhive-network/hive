#pragma once
#include <hive/plugins/json_rpc/utility.hpp>

#include <hive/protocol/operations.hpp>
#include <hive/protocol/types.hpp>

#include <hive/plugins/account_history_api/annotated_signed_transaction.hpp>

#include <hive/chain/buffer_type.hpp>

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
    op_in_trx( op_obj.op_in_trx ),
    timestamp( op_obj.timestamp )
  {
    op = fc::raw::unpack_from_buffer< hive::protocol::operation >( op_obj.serialized_op );
    virtual_op = hive::protocol::is_virtual_operation(op);
  }

  hive::protocol::transaction_id_type trx_id;
  uint32_t                            block = 0;
  uint32_t                            trx_in_block = 0;
  uint32_t                            op_in_trx = 0;
  bool                                virtual_op = false;
  uint64_t                            operation_id = 0;
  fc::time_point_sec                  timestamp;
  hive::protocol::operation           op;

  bool operator<( const api_operation_object& obj ) const
  {
    return std::tie( block, trx_in_block, op_in_trx ) < std::tie( obj.block, obj.trx_in_block, obj.op_in_trx );
  }
};


struct get_ops_in_block_args
{
  uint32_t block_num = 0;
  bool     only_virtual = false;
  /// if set to true also operations from reversible block will be included if block_num points to such block.
  fc::optional<bool> include_reversible;
};

struct get_ops_in_block_return
{
  std::multiset< api_operation_object > ops;
};


struct get_transaction_args
{
  fc::string id;
  /// if set to true transaction from reversible block will be returned if id matches given one.
  fc::optional<bool> include_reversible;
};

typedef hive::plugins::account_history::annotated_signed_transaction get_transaction_return;

struct get_account_history_args
{
  hive::protocol::account_name_type   account;
  uint64_t                            start = -1;
  uint32_t                            limit = 1000;
  /// if set to true operations from reversible block will be also returned.
  fc::optional<bool> include_reversible;
  /** if either are set, the set of returned operations will include only these
   * matching bitwise filter.
   * For the first 64 operations (as defined in protocol/operations.hpp), set the
   * corresponding bit in operation_filter_low; for the higher-numbered operations,
   * set the bit in operation_filter_high (pretending operation_filter is a
   * 128-bit bitmask composed of {operation_filter_high, operation_filter_low})
   */
  fc::optional<uint64_t> operation_filter_low;
  fc::optional<uint64_t> operation_filter_high;
};

struct get_account_history_return
{
  std::map< uint32_t, api_operation_object > history;
};

enum enum_vops_filter : uint64_t
{
  fill_convert_request_operation                = 0x0000'00000001ull,
  author_reward_operation                       = 0x0000'00000002ull,
  curation_reward_operation                     = 0x0000'00000004ull,
  comment_reward_operation                      = 0x0000'00000008ull,
  liquidity_reward_operation                    = 0x0000'00000010ull,
  interest_operation                            = 0x0000'00000020ull,
  fill_vesting_withdraw_operation               = 0x0000'00000040ull,
  fill_order_operation                          = 0x0000'00000080ull,
  shutdown_witness_operation                    = 0x0000'00000100ull,
  fill_transfer_from_savings_operation          = 0x0000'00000200ull,
  hardfork_operation                            = 0x0000'00000400ull,
  comment_payout_update_operation               = 0x0000'00000800ull,
  return_vesting_delegation_operation           = 0x0000'00001000ull,
  comment_benefactor_reward_operation           = 0x0000'00002000ull,
  producer_reward_operation                     = 0x0000'00004000ull,
  clear_null_account_balance_operation          = 0x0000'00008000ull,
  proposal_pay_operation                        = 0x0000'00010000ull,
  dhf_funding_operation                         = 0x0000'00020000ull,
  hardfork_hive_operation                       = 0x0000'00040000ull,
  hardfork_hive_restore_operation               = 0x0000'00080000ull,
  delayed_voting_operation                      = 0x0000'00100000ull,
  consolidate_treasury_balance_operation        = 0x0000'00200000ull,
  effective_comment_vote_operation              = 0x0000'00400000ull,
  ineffective_delete_comment_operation          = 0x0000'00800000ull,
  dhf_conversion_operation                      = 0x0000'01000000ull,
  expired_account_notification_operation        = 0x0000'02000000ull,
  changed_recovery_account_operation            = 0x0000'04000000ull,
  transfer_to_vesting_completed_operation       = 0x0000'08000000ull,
  pow_reward_operation                          = 0x0000'10000000ull,
  vesting_shares_split_operation                = 0x0000'20000000ull,
  account_created_operation                     = 0x0000'40000000ull,
  fill_collateralized_convert_request_operation = 0x0000'80000000ull,
  system_warning_operation                      = 0x0001'00000000ull,
  fill_recurrent_transfer_operation             = 0x0002'00000000ull,
  failed_recurrent_transfer_operation           = 0x0004'00000000ull,
  limit_order_cancelled_operation               = 0x0008'00000000ull,
  producer_missed_operation                     = 0x0010'00000000ull,
  proposal_fee_operation                        = 0x0020'00000000ull,
  collateralized_convert_immediate_conversion_operation
                                                = 0x0040'00000000ull,
  escrow_approved_operation                     = 0x0080'00000000ull,
  escrow_rejected_operation                     = 0x0100'00000000ull,
  proxy_cleared_operation                       = 0x0200'00000000ull,
  declined_voting_rights                        = 0x0400'00000000ull,
};

/** Allows to specify range of blocks to retrieve virtual operations for.
  *  \param block_range_begin - starting block number (inclusive) to search for virtual operations
  *  \param block_range_end   - last block number (exclusive) to search for virtual operations
  *  \param operation_begin   - starting virtual operation in given block (inclusive)
  *  \param limit             - a limit of retrieved operations
  *  \param filter            - a filter that decides which an operation matches - used bitwise filtering equals to position in `hive::protocol::operation`
  */
struct enum_virtual_ops_args
{
  uint32_t block_range_begin = 1;
  uint32_t block_range_end = 2;
  /// if set to true operations from reversible block will be also returned.
  fc::optional<bool> include_reversible;

  fc::optional<bool> group_by_block;
  fc::optional< uint64_t > operation_begin;
  fc::optional< int32_t > limit;
  fc::optional< uint64_t > filter;
};

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

    DECLARE_API(
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

FC_REFLECT( hive::plugins::account_history::get_ops_in_block_return,
  (ops) )

FC_REFLECT( hive::plugins::account_history::get_transaction_args,
  (id)(include_reversible) )

FC_REFLECT( hive::plugins::account_history::get_account_history_args,
  (account)(start)(limit)(include_reversible)(operation_filter_low)(operation_filter_high))

FC_REFLECT( hive::plugins::account_history::get_account_history_return,
  (history) )

FC_REFLECT( hive::plugins::account_history::enum_virtual_ops_args,
  (block_range_begin)(block_range_end)(include_reversible)(group_by_block)(operation_begin)(limit)(filter) )

FC_REFLECT( hive::plugins::account_history::ops_array_wrapper, (block)(irreversible)(timestamp)(ops) )

FC_REFLECT( hive::plugins::account_history::enum_virtual_ops_return,
  (ops)(ops_by_block)(next_block_range_begin)(next_operation_begin) )
