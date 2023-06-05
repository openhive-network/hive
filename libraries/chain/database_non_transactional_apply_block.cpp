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
    _non_transactional_apply_block(full_block, ops);
  } );

}

void database::_non_transactional_apply_block(const std::shared_ptr<full_block_type>& full_block, const std::vector<std::vector<char>>& ops)
{
  const signed_block& block = full_block->get_block();
  const uint32_t block_num = full_block->get_block_num();
  block_notification note(full_block);

  try {
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
    process_genesis_accounts();

    // For every existing before the head_block_time (genesis time), apply the hardfork
    // This allows the test net to launch with past hardforks and apply the next harfork when running

    uint32_t n;
    for( n=0; n<HIVE_NUM_HARDFORKS; n++ )
    {
      if( _hardfork_versions.times[n+1] > block.timestamp )
        break;
    }

    if( n > 0 )
    {
      ilog( "Processing ${n} genesis hardforks", ("n", n) );
      set_hardfork( n, true );
#ifdef IS_TEST_NET
      if( n < HIVE_NUM_HARDFORKS )
      {
        ilog( "Next hardfork scheduled for ${t} (current block timestamp ${c})",
          ( "t", _hardfork_versions.times[ n + 1 ] )( "c", block.timestamp ) );
      }
#endif

      const hardfork_property_object& hardfork_state = get_hardfork_property_object();
      FC_ASSERT( hardfork_state.current_hardfork_version == _hardfork_versions.versions[n], "Unexpected genesis hardfork state" );

      const auto& witness_idx = get_index<witness_index>().indices().get<by_id>();
      vector<witness_id_type> wit_ids_to_update;
      for( auto it=witness_idx.begin(); it!=witness_idx.end(); ++it )
        wit_ids_to_update.push_back( it->get_id() );

      for( witness_id_type wit_id : wit_ids_to_update )
      {
        modify( get( wit_id ), [&]( witness_object& wit )
        {
          wit.running_version = _hardfork_versions.versions[n];
          wit.hardfork_version_vote = _hardfork_versions.versions[n];
          wit.hardfork_time_vote = _hardfork_versions.times[n];
        } );
      }
    }
  }

  if( !( skip & skip_merkle_check ) )
  {
    if( _benchmark_dumper.is_enabled() )
      _benchmark_dumper.begin();

    auto merkle_root = full_block->get_merkle_root();

    try
    {
      FC_ASSERT(block.transaction_merkle_root == merkle_root, "Merkle check failed",
                (block.transaction_merkle_root)(merkle_root)(block)("id", full_block->get_block_id()));
    }
    catch( fc::assert_exception& e )
    { //don't throw error if this is a block with a known bad merkle root
      const auto& merkle_map = get_shared_db_merkle();
      auto itr = merkle_map.find( block_num );
      if( itr == merkle_map.end() || itr->second != merkle_root )
        throw e;
    }

    if( _benchmark_dumper.is_enabled() )
      _benchmark_dumper.end( "block", "merkle check" );
  }

  const witness_object& signing_witness = validate_block_header(skip, full_block);

  const auto& gprops = get_dynamic_global_properties();
  uint32_t block_size = full_block->get_uncompressed_block().raw_size;
  if( has_hardfork( HIVE_HARDFORK_0_12 ) )
    FC_ASSERT(block_size <= gprops.maximum_block_size, "Block size is larger than voted on block size", (block_num)(block_size)("max",gprops.maximum_block_size));

  if( block_size < HIVE_MIN_BLOCK_SIZE )
    elog("Block size is too small", (block_num)(block_size)("min", HIVE_MIN_BLOCK_SIZE));

  /// modify current witness so transaction evaluators can know who included the transaction,
  /// this is mostly for POW operations which must pay the current_witness
  modify( gprops, [&]( dynamic_global_property_object& dgp ){
    dgp.current_witness = block.witness;
  });

  required_automated_actions req_actions;
  optional_automated_actions opt_actions;
  /// parse witness version reporting
  process_header_extensions( block, req_actions, opt_actions );

  if( has_hardfork( HIVE_HARDFORK_0_5__54 ) ) // Cannot remove after hardfork
  {
    const auto& witness = get_witness( block.witness );
    const auto& hardfork_state = get_hardfork_property_object();
    FC_ASSERT(witness.running_version >= hardfork_state.current_hardfork_version,
              "Block produced by witness that is not running current hardfork",
              (witness)(block.witness)(hardfork_state));
  }
    // process the operations
    for(const auto& op_vector : ops)
    {
        const char* raw_data = op_vector.data();
        uint32_t data_length = op_vector.size();

        hive::protocol::operation op = fc::raw::unpack_from_char_array<hive::protocol::operation>(raw_data, data_length);

        //_current_op_in_trx = 0;
        try {
            apply_operation(op);
            //++_current_op_in_trx;
        } FC_CAPTURE_AND_RETHROW( (op) )
    }

  _current_trx_in_block = -1;
  _current_op_in_trx = 0;

  update_global_dynamic_data(block);
  update_signing_witness(signing_witness, block);

  uint32_t old_last_irreversible = update_last_irreversible_block(true);

  create_block_summary(full_block);
  clear_expired_transactions();
  clear_expired_orders();
  clear_expired_delegations();

  update_witness_schedule(*this);

  update_median_feed();
  update_virtual_supply(); //accommodate potentially new price

  clear_null_account_balance();
  consolidate_treasury_balance();
  process_funds();
  process_conversions();
  process_comment_cashout();
  process_vesting_withdrawals();
  process_savings_withdraws();
  process_subsidized_accounts();
  pay_liquidity_reward();
  update_virtual_supply(); //cover changes in HBD supply from above processes

  account_recovery_processing();
  expire_escrow_ratification();
  process_decline_voting_rights();
  process_proposals( note ); //new HBD converted here does not count towards limit
  process_delayed_voting( note );
  remove_expired_governance_votes();

  process_recurrent_transfers();

  generate_required_actions();
  generate_optional_actions();

  process_required_actions( req_actions );
  process_optional_actions( opt_actions );

  process_hardforks();

  // notify observers that the block has been applied
  notify_post_apply_block( note );

  notify_changed_objects();

  // This moves newly irreversible blocks from the fork db to the block log
  // and commits irreversible state to the database. This should always be the
  // last call of applying a block because it is the only thing that is not
  // reversible.
  migrate_irreversible_state(old_last_irreversible);
} FC_CAPTURE_CALL_LOG_AND_RETHROW( std::bind( &database::notify_fail_apply_block, this, note ), (block_num) ) }

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