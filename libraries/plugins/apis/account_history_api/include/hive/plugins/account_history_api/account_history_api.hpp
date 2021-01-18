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
  api_operation_object( const T& op_obj, bool/*not used, but necessary so as to distinguish constructors*/ ) :
    trx_id( op_obj.trx_id ),
    block( op_obj.block ),
    trx_in_block( op_obj.trx_in_block ),
    virtual_op( op_obj.virtual_op ),
    timestamp( op_obj.timestamp ),
    op( op_obj.op )
  {
  }

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
  uint32_t                               block = 0;
  uint32_t                               trx_in_block = 0;
  uint32_t                               op_in_trx = 0;
  uint32_t                               virtual_op = 0;
  uint64_t                               operation_id = 0;
  fc::time_point_sec                     timestamp;
  hive::protocol::operation             op;

  bool operator<( const api_operation_object& obj ) const
  {
    return std::tie( block, trx_in_block, op_in_trx, virtual_op ) < std::tie( obj.block, obj.trx_in_block, obj.op_in_trx, obj.virtual_op );
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
  hive::protocol::transaction_id_type id;
  /// if set to true transaction from reversible block will be returned if id matches given one.
  fc::optional<bool> include_reversible;
};

typedef hive::protocol::annotated_signed_transaction get_transaction_return;

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

enum enum_vops_filter : uint32_t
{
  fill_convert_request_operation          = 0x000001,
  author_reward_operation                 = 0x000002,
  curation_reward_operation               = 0x000004,
  comment_reward_operation                = 0x000008,
  liquidity_reward_operation              = 0x000010,
  interest_operation                      = 0x000020,
  fill_vesting_withdraw_operation         = 0x000040,
  fill_order_operation                    = 0x000080,
  shutdown_witness_operation              = 0x000100,
  fill_transfer_from_savings_operation    = 0x000200,
  hardfork_operation                      = 0x000400,
  comment_payout_update_operation         = 0x000800,
  return_vesting_delegation_operation     = 0x001000,
  comment_benefactor_reward_operation     = 0x002000,
  producer_reward_operation               = 0x004000,
  clear_null_account_balance_operation    = 0x008000,
  proposal_pay_operation                  = 0x010000,
  sps_fund_operation                      = 0x020000,
  hardfork_hive_operation                 = 0x040000,
  hardfork_hive_restore_operation         = 0x080000,
  delayed_voting_operation                = 0x100000,
  consolidate_treasury_balance_operation  = 0x200000,
  effective_comment_vote_operation        = 0x400000,
  ineffective_delete_comment_operation    = 0x800000,
  sps_convert_operation                   = 0x1000000
};

/** Allows to specify range of blocks to retrieve virtual operations for.
  *  \param block_range_begin - starting block number (inclusive) to search for virtual operations
  *  \param block_range_end   - last block number (exclusive) to search for virtual operations
  *  \param operation_begin   - starting virtual operation in given block (inclusive)
  *  \param limit             - a limit of retrieved operations
  *  \param block_range_end   - a filter that decides which an operation matches - used bitwise filtering equals to position in `hive::protocol::operation`
  */
struct enum_virtual_ops_args
{
  uint32_t block_range_begin = 1;
  uint32_t block_range_end = 2;
  /// if set to true operations from reversible block will be also returned.
  fc::optional<bool> include_reversible;

  fc::optional<bool> group_by_block;
  fc::optional< uint64_t > operation_begin;
  fc::optional< uint32_t > limit;
  fc::optional< uint32_t > filter;
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
  std::set<ops_array_wrapper> ops_by_block;
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
  (block_num)(only_virtual) )

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
