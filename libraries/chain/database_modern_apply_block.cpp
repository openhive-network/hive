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

void database::modern_apply_block(const std::shared_ptr<full_block_type>& full_block, pqxx::result::const_iterator& current_operation, uint32_t skip)
{
  const signed_block& block = full_block->get_block();
  const uint32_t block_num = full_block->get_block_num();
  block_notification note(full_block);

  try 
  {
    notify_pre_apply_block( note );
    
    BOOST_SCOPE_EXIT( this_ )
    {
      this_->_currently_processing_block_id.reset();
    } BOOST_SCOPE_EXIT_END
    
    _currently_processing_block_id = full_block->get_block_id();

    uint32_t skip = get_node_properties().skip_flags;
    _current_block_num    = block_num;
    _current_trx_in_block = 0;

    if( BOOST_UNLIKELY( block_num == 1 ) )
    {
      process_hardforks_at_genesis(block_num, block);
    }

    process_merkle_check(block, skip, block_num, full_block);

    const witness_object& signing_witness = validate_block_header(skip, full_block);

    const auto& gprops = get_dynamic_global_properties();
    uint32_t block_size = full_block->get_uncompressed_block().raw_size;

    required_automated_actions req_actions;
    optional_automated_actions opt_actions;

    update_blockchain_state(block, signing_witness, block_size, block_num, req_actions, opt_actions);
    process_transactions(full_block, skip);

    _current_trx_in_block = -1;
    _current_op_in_trx = 0;

    uint32_t old_last_irreversible;
    execute_operations_after_block_application(full_block, note, signing_witness, block, old_last_irreversible, req_actions, opt_actions );

    finalize_block_application(note, old_last_irreversible);
  } 
  FC_CAPTURE_CALL_LOG_AND_RETHROW( std::bind( &database::notify_fail_apply_block, this, note ), (block_num) )
}

}}