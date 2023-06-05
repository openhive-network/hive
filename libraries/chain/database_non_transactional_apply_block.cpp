#include <hive/chain/hive_fwd.hpp>

#include <appbase/application.hpp>

#include <hive/protocol/hive_operations.hpp>
#include <hive/protocol/get_config.hpp>
#include <hive/protocol/transaction_util.hpp>

#include <hive/chain/block_summary_object.hpp>
#include <hive/chain/compound.hpp>
#include <hive/chain/custom_operation_interpreter.hpp>
#include <hive/chain/database.hpp>
#include <hive/chain/database_exceptions.hpp>
#include <hive/chain/db_with.hpp>
#include <hive/chain/evaluator_registry.hpp>
#include <hive/chain/global_property_object.hpp>
#include <hive/chain/optional_action_evaluator.hpp>
#include <hive/chain/pending_required_action_object.hpp>
#include <hive/chain/pending_optional_action_object.hpp>
#include <hive/chain/required_action_evaluator.hpp>
#include <hive/chain/smt_objects.hpp>
#include <hive/chain/hive_evaluator.hpp>
#include <hive/chain/hive_objects.hpp>
#include <hive/chain/transaction_object.hpp>
#include <hive/chain/shared_db_merkle.hpp>
#include <hive/chain/witness_schedule.hpp>
#include <hive/chain/blockchain_worker_thread_pool.hpp>

#include <hive/chain/util/asset.hpp>
#include <hive/chain/util/reward.hpp>
#include <hive/chain/util/uint256.hpp>
#include <hive/chain/util/reward.hpp>
#include <hive/chain/util/manabar.hpp>
#include <hive/chain/util/rd_setup.hpp>
#include <hive/chain/util/nai_generator.hpp>
#include <hive/chain/util/dhf_processor.hpp>
#include <hive/chain/util/delayed_voting.hpp>

#include <fc/smart_ref_impl.hpp>
#include <fc/uint128.hpp>

#include <fc/container/deque.hpp>

#include <fc/io/fstream.hpp>

#include <boost/scope_exit.hpp>
#include <boost/range/adaptor/reversed.hpp>

#include <iostream>

#include <cstdint>
#include <deque>
#include <fstream>
#include <functional>

#include <stdlib.h>

namespace hive { namespace chain {

void database::non_transactional_apply_block(const std::shared_ptr<full_block_type>& full_block, const std::vector<std::vector<char>>& ops, uint32_t skip)
{

  detail::with_skip_flags( *this, skip, [&]()
  {
    _apply_block(full_block, [this, ops](const std::shared_ptr<full_block_type>& full_block, uint32_t skip){ _process_operations(ops);});
    
  } );

}


void database::_process_operations(const std::vector<std::vector<char>>& ops)
{
  // process the operations
  for(const auto& op_vector : ops)
  {
    const char* raw_data = op_vector.data();
    uint32_t data_length = op_vector.size();

    hive::protocol::operation op =
        fc::raw::unpack_from_char_array<hive::protocol::operation>(raw_data, data_length);

    //_current_op_in_trx = 0;
    try
    {
      apply_operation(op);
      //++_current_op_in_trx;
    }
    FC_CAPTURE_AND_RETHROW((op))
  }
  // end process the operations
}

void database::non_transactional_update_blockchain_state(const signed_block& sign_block, uint32_t block_size, int block_num, required_automated_actions& req_actions, optional_automated_actions& opt_actions)
{
    const auto& gprops = get_dynamic_global_properties();
    
    if( has_hardfork( HIVE_HARDFORK_0_12 ) )
        FC_ASSERT(block_size <= gprops.maximum_block_size, "Block size is larger than voted on block size", (block_num)(block_size)("max",gprops.maximum_block_size));

    if( block_size < HIVE_MIN_BLOCK_SIZE )
        elog("Block size is too small", (block_num)(block_size)("min", HIVE_MIN_BLOCK_SIZE));

    /// modify current witness so transaction evaluators can know who included the transaction,
    /// this is mostly for POW operations which must pay the current_witness
    modify( gprops, [&]( dynamic_global_property_object& dgp ){
        dgp.current_witness = sign_block.witness;
    });

    
    /// parse witness version reporting
    process_header_extensions( sign_block, req_actions, opt_actions );

    if( has_hardfork( HIVE_HARDFORK_0_5__54 ) ) // Cannot remove after hardfork
    {
        const auto& witness = get_witness( sign_block.witness );
        const auto& hardfork_state = get_hardfork_property_object();
        FC_ASSERT(witness.running_version >= hardfork_state.current_hardfork_version,
            "Block produced by witness that is not running current hardfork",
            (witness)(sign_block.witness)(hardfork_state));
    }
}


}}