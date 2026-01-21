add_subdirectory(db_fixture)
add_subdirectory(bmic_objects)

find_package( Gperftools QUIET )
if( GPERFTOOLS_FOUND )
    message( STATUS "Found gperftools; compiling tests with TCMalloc")
    list( APPEND PLATFORM_SPECIFIC_LIBS tcmalloc )
endif()

INCLUDE(CTest)

ENABLE_TESTING()

file(GLOB UNIT_TESTS
   "tests/*.cpp"
)

include(BoostTestTargets.cmake)

function(parse_unit_tests RESULT)
   set(SOURCES)
	foreach(_element ${ARGN})
      list(APPEND SOURCES "${_element}")
	endforeach()

   set(tests)

   foreach(src ${SOURCES})
      file(READ ${src} thefile)
      #string(REGEX MATCH "" test_suite "${thefile}" )
      string(REGEX MATCH "BOOST_FIXTURE_TEST_SUITE\\([A-Za-z0-9_, ]*\\)" test_suite "${thefile}" )

      if( NOT (test_suite STREQUAL "") )
         string(SUBSTRING "${test_suite}" 25 -1 test_suite)
         string(FIND "${test_suite}" "," comma_loc )
         string(SUBSTRING "${test_suite}" 0 ${comma_loc} test_suite)
         string(STRIP "${test_suite}" test_suite)

         string( REGEX MATCHALL "BOOST_AUTO_TEST_CASE\\([A-Za-z0-9_, ]*\\)" cases "${thefile}" )

         foreach( test_case ${cases} )
            string(SUBSTRING "${test_case}" 22 -1 test_case)
            string(FIND "${test_case}" ")" paren_loc )
            string(SUBSTRING "${test_case}" 0 ${paren_loc} test_case)
            string(STRIP "${test_case}" test_case)

            list(APPEND tests "${test_suite}/${test_case}")
         endforeach()
      endif()
   endforeach()

   set(${RESULT} ${tests} PARENT_SCOPE)
endfunction()

parse_unit_tests(TEST_CASES ${UNIT_TESTS})

#message(STATUS "${TEST_CASES}")

# Test name can optionally be followed by :cost to influence CTest scheduling.
# Higher costs tests start first. E.g., "test_name:721"
add_boost_test( chain_test
   SOURCES ${UNIT_TESTS}
   TESTS
   basic_tests/additional_allocations
   basic_tests/adjust_balance_test
   basic_tests/authorization_redirections
   basic_tests/authorization_speed
   basic_tests/chain_object_checksum
   basic_tests/chain_object_size
   basic_tests/create_array_from_fc_json
   basic_tests/curation_weight_test
   basic_tests/decoded_type_data_json_operations
   basic_tests/decoding_types_mechanism_test
   basic_tests/fc_json_integer_formatting
   basic_tests/fc_json_is_valid
   basic_tests/fc_optional_alignment
   basic_tests/fc_static_variant_alignment
   basic_tests/fc_uint128_to_string
   basic_tests/fixed_string_verification
   basic_tests/get_word_list
   basic_tests/merkle_root
   basic_tests/parse_size_test
   basic_tests/pow2_equihash
   basic_tests/valid_name_test
   block_storage_tests/auto_split
   block_storage_tests/auto_split_2
   block_storage_tests/auto_split_3
   block_storage_tests/auto_split_4
   block_storage_tests/clean_replay_legacy_to_split
   block_storage_tests/clean_replay_legacy_to_zero
   block_storage_tests/clean_replay_split_to_effectively_pruned
   block_storage_tests/get_block_range
   block_storage_tests/incremental_replay_pruned
   block_storage_tests/legacy_to_split
   block_storage_tests/legacy_to_zero
   block_storage_tests/replay_legacy_to_split
   block_storage_tests/replay_legacy_to_zero
   block_storage_tests/snapshot_legacy_to_zero
   block_storage_tests/snapshot_pruned_to_pruned
   block_storage_tests/snapshot_replay_legacy_to_split
   block_storage_tests/snapshot_replay_legacy_to_zero
   block_storage_tests/snapshot_replay_pruned_to_pruned
   block_storage_tests/snapshot_replay_split_to_split
   block_storage_tests/snapshot_replay_split_to_zero
   block_storage_tests/snapshot_zero_to_zero
   block_storage_tests/split_over_split
   block_storage_tests/split_to_zero
   block_tests/block_flow_control_generation
   block_tests/block_flow_control_p2p
   block_tests/double_sign_check
   block_tests/duplicate_transactions
   block_tests/fast_confirm_plus_out_of_order_blocks
   block_tests/fork_blocks
   block_tests/generate_block_size
   block_tests/generate_empty_blocks
   block_tests/hardfork_test
   block_tests/init_hive_hbd_supply
   block_tests/optional_tapos
   block_tests/pop_block_twice
   block_tests/rsf_missed_blocks
   block_tests/safe_closing_database
   block_tests/set_lower_lib_then_current
   block_tests/skip_block
   block_tests/switch_forks_undo_create
   block_tests/switch_forks_using_fast_confirm
   block_tests/tapos
   block_tests/undo_block
   bmic_tests/basic_tests
   bmic_tests/insert_remove_collision_tests
   bmic_tests/insert_remove_tests
   bmic_tests/misc_tests
   bmic_tests/misc_tests3
   bmic_tests/modify_tests
   comments_in_external_storage/basic_checks
   comments_in_external_storage/fork_reverts_cashout
   comments_in_external_storage/inconsistent_comment_archive
   comments_in_external_storage/nested_comments
   config_tests/custom_logging_config_test
   config_tests/default_logging_config_test
   config_tests/rotation_test
   curation_reward_tests/basic_test_v0
   curation_reward_tests/basic_test_v1
   curation_reward_tests/basic_test_v2
   curation_reward_tests/basic_test_v3
   curation_reward_tests/basic_test_v4
   curation_reward_tests/basic_test_v5
   curation_reward_tests/basic_test_v6
   curation_reward_tests/basic_test_v7
   curation_reward_tests/basic_test_v8
   curation_reward_tests2/five_votes_per_comment
   curation_reward_tests/no_votes
   curation_reward_tests/one_vote_for_comment
   curation_reward_tests2/one_vote_per_comment
   curation_reward_tests2/one_vote_per_comment_v2
   curation_reward_tests2/two_comments_in_different_blocks
   curation_reward_tests2/two_comments_in_different_blocks_v2
   curation_reward_tests2/two_comments_in_the_same_blocks
   curation_reward_tests2/two_comments_in_the_same_blocks_v2
   curation_reward_tests/two_votes_for_comment
   delayed_voting_proposal_tests/delayed_proposal_test_01
   delayed_voting_proposal_tests/delayed_proposal_test_02
   delayed_voting_tests/decline_voting_rights_01
   delayed_voting_tests/decline_voting_rights_02
   delayed_voting_tests/decline_voting_rights_03
   delayed_voting_tests/decline_voting_rights_04
   delayed_voting_tests/delayed_voting_01
   delayed_voting_tests/delayed_voting_04
   delayed_voting_tests/delayed_voting_05
   delayed_voting_tests/delayed_voting_06
   delayed_voting_tests/delayed_voting_basic_01
   delayed_voting_tests/delayed_voting_basic_02
   delayed_voting_tests/delayed_voting_basic_03
   delayed_voting_tests/delayed_voting_basic_04
   delayed_voting_tests/delayed_voting_basic_05
   delayed_voting_tests/delayed_voting_basic_06
   delayed_voting_tests/delayed_voting_many_vesting_01
   delayed_voting_tests/delayed_voting_processor_01
   delayed_voting_tests/delayed_voting_processor_02
   delayed_voting_tests/delayed_voting_processor_03
   delayed_voting_tests/delayed_voting_proxy_01
   delayed_voting_tests/delayed_voting_proxy_02
   delayed_voting_tests/scenario_01
   delayed_voting_tests/small_common_test_01
   hf28_tests2/artificial_1_on_power_down
   hf28_tests/basic_expiration_test
   hf23_tests/basic_test_01
   hf23_tests/basic_test_02
   hf23_tests/basic_test_03
   hf23_tests/basic_test_04
   hf23_tests/basic_test_05
   hf23_tests/basic_test_06
   hf24_tests/blocked_operations
   hf24_tests/comment_beneficiary
   hf24_tests/consolidate_balance
   hf23_tests/convert_request_cleanup_test
   hf28_tests/declined_voting_rights_basic
   hf28_tests/declined_voting_rights_basic_2
   hf28_tests/declined_voting_rights_between_hf27_and_hf28
   hf28_tests/declined_voting_rights_between_hf27_and_hf28_2
   hf28_tests/declined_voting_rights_proposal_votes
   hf28_tests/declined_voting_rights_proposal_votes_2
   direct_rc_delegation/delegate_rc_operation_apply_many
   direct_rc_delegation/delegate_rc_operation_apply_many_different
   direct_rc_delegation/delegate_rc_operation_apply_single
   direct_rc_delegation/delegate_rc_operation_validate
   hf28_tests/different_behaviour_for_nonexistent_proposals
   direct_rc_delegation/direct_rc_delegation_vesting_withdrawal
   direct_rc_delegation/direct_rc_delegation_vesting_withdrawal_routes
   hf28_tests2/disturbed_power_down
   hf28_tests2/empty_voting
   hf23_tests/escrow_cleanup_test
   hf28_tests2/genesis_account_authorities
   hf28_tests2/global_witness_props_basic_test
   hf28_tests2/global_witness_props_change_applied_after_hf_test
   hf28_tests2/global_witness_props_change_noticed_after_hf_test
   hf23_tests/hbd_test_01
   hf23_tests/hbd_test_02
   hf23_tests/limit_order_cleanup_test
   hf28_tests/mixed_authorities_in_one_transaction
   hf28_tests/mixed_authorities_in_one_transaction_2
   hf26_tests/owner_update_limit
   hf26_tests/pack_transaction_basic
   direct_rc_delegation/rc_delegation_regeneration
   direct_rc_delegation/rc_delegation_removal_no_rc:171
   direct_rc_delegation/rc_negative_regeneration_bug
   hf23_tests/restore_accounts_01
   hf23_tests/restore_accounts_02
   hf23_tests/save_test_01
   hf23_tests/save_test_02
   hf23_tests/savings_test_01
   hf23_tests/savings_test_02
   hf24_tests/treasury_debt_ratio
   hf28_tests2/treasury_hbd_does_not_affect_inflation_advanced
   hf28_tests2/treasury_hbd_does_not_affect_inflation_basic
   hf26_tests/update_operation
   direct_rc_delegation/update_outdel_overflow
   direct_rc_delegation/update_outdel_overflow_delegatee
   direct_rc_delegation/update_outdel_overflow_delegatee_performance
   direct_rc_delegation/update_outdel_overflow_many_accounts
   hf28_tests/verify_authority_limits
   hf28_tests/verify_authority_limits_for_temp_account
   hf28_tests2/vote_edit_limit
   hf28_tests2/vote_stabilization
   operation_tests/account_create_validate
   operation_tests/account_create_authorities
   operation_tests/account_create_apply
   operation_tests/account_update_validate
   operation_tests/account_update_authorities
   operation_tests/account_update_apply
   operation_tests/comment_validate
   operation_tests/comment_authorities
   operation_tests/comment_apply
   operation_tests/comment_delete_apply
   operation_tests/vote_validate
   operation_tests/vote_authorities
   operation_tests/vote_apply
   operation_tests/vote_weights
   operation_tests/transfer_validate
   operation_tests/transfer_authorities
   operation_tests/signature_stripping
   operation_tests/transfer_apply
   operation_tests/transfer_to_vesting_validate
   operation_tests/transfer_to_vesting_authorities
   operation_tests/transfer_to_vesting_apply
   operation_tests/withdraw_vesting_validate
   operation_tests/withdraw_vesting_authorities
   operation_tests/withdraw_vesting_apply
   operation_tests/witness_update_validate
   operation_tests/witness_update_authorities
   operation_tests/witness_update_apply
   operation_tests/account_witness_vote_validate
   operation_tests/account_witness_vote_authorities
   operation_tests/account_witness_vote_apply
   operation_tests/account_witness_proxy_validate
   operation_tests/account_witness_proxy_authorities
   operation_tests/account_witness_proxy_apply
   operation_tests/account_witness_vote_apply_delay
   operation_tests/account_witness_proxy_apply_delay
   operation_tests/account_witness_proxy_too_long
   operation_tests/proxy_cleared_operation_basic
   operation_tests/account_witness_block_approve_authorities
   operation_tests/custom_authorities
   operation_tests/custom_json_authorities
   operation_tests/custom_json_rate_limit
   operation_tests/custom_binary_authorities
   operation_tests/feed_publish_validate
   operation_tests/feed_publish_authorities
   operation_tests/feed_publish_apply
   operation_tests/convert_validate
   operation_tests/convert_authorities
   operation_tests/convert_apply
   operation_tests/collateralized_convert_authorities
   operation_tests/collateralized_convert_apply
   operation_tests/collateralized_convert_narrow_price
   operation_tests/collateralized_convert_wide_price
   operation_tests/limit_order_create_validate
   operation_tests/limit_order_create_authorities
   operation_tests/limit_order_create_apply
   operation_tests/limit_order_create2_authorities
   operation_tests/limit_order_create2_apply
   operation_tests/limit_order_cancel_validate
   operation_tests/limit_order_cancel_authorities
   operation_tests/limit_order_cancel_apply
   operation_tests/pow_validate
   operation_tests/pow_authorities
   operation_tests/pow_apply
   operation_tests/account_recovery
   operation_tests/change_recovery_account
   operation_tests/escrow_transfer_validate
   operation_tests/escrow_transfer_authorities
   operation_tests/escrow_transfer_apply
   operation_tests/escrow_approve_validate
   operation_tests/escrow_approve_authorities
   operation_tests/escrow_approve_apply
   operation_tests/escrow_dispute_validate
   operation_tests/escrow_dispute_authorities
   operation_tests/escrow_dispute_apply
   operation_tests/escrow_release_validate
   operation_tests/escrow_release_authorities
   operation_tests/escrow_release_apply
   operation_tests/escrow_limit
   operation_tests/transfer_to_savings_validate
   operation_tests/transfer_to_savings_authorities
   operation_tests/transfer_to_savings_apply
   operation_tests/transfer_from_savings_validate
   operation_tests/transfer_from_savings_authorities
   operation_tests/transfer_from_savings_apply
   operation_tests/cancel_transfer_from_savings_validate
   operation_tests/cancel_transfer_from_savings_authorities
   operation_tests/cancel_transfer_from_savings_apply
   operation_tests/decline_voting_rights_authorities
   operation_tests/decline_voting_rights_apply
   operation_tests/claim_reward_balance_validate
   operation_tests/claim_reward_balance_authorities
   operation_tests/account_create_with_delegation_authorities
   operation_tests/account_create_with_delegation_apply
   operation_tests/claim_reward_balance_apply
   operation_tests/delegate_vesting_shares_validate
   operation_tests/delegate_vesting_shares_authorities
   operation_tests/delegate_vesting_shares_apply
   operation_tests/issue_971_vesting_removal
   operation_tests/comment_beneficiaries_validate
   operation_tests/comment_beneficiaries_apply
   operation_tests/comment_options_apply
   operation_tests/comment_options_deleted_permlink_reuse
   operation_tests/message_when_author_or_comment_doesnt_exist
   operation_tests/witness_set_properties_validate
   operation_tests/witness_set_properties_authorities
   operation_tests/witness_set_properties_apply
   operation_tests/claim_account_validate
   operation_tests/claim_account_authorities
   operation_tests/claim_account_apply
   operation_tests/create_claimed_account_validate
   operation_tests/create_claimed_account_authorities
   operation_tests/create_claimed_account_apply
   operation_tests/account_auth_tests
   operation_tests/account_object_by_governance_vote_expiration_ts_idx
   operation_tests/account_update2_validate
   operation_tests/account_update2_authorities
   operation_tests/account_update2_apply
   operation_tests/recurrent_transfer_validate
   operation_tests/recurrent_transfer_max_transfer_processed_per_block:112
   operation_tests/recurrent_transfer_max_open_transfers:112
   operation_tests/recurrent_transfer_hbd
   operation_tests/recurrent_transfer_apply
   operation_tests/recurrent_transfer_pair_id_basic
   operation_tests/recurrent_transfer_pair_id_crud
   operation_tests/recurrent_transfer_same_pair_id_different_receivers
   operation_tests/recurrent_transfer_exact_max
   operation_tests/recurrent_transfer_edit_overextended
   operation_tests/recurrent_transfer_full_edit_overextended
   operation_tests/failed_recurrent_transfer
   operation_tests/extensions_in_virtual_operations_generated_after_recurrent_transfer
   operation_tests/private_key_memo_test
   operation_time_tests/comment_payout_equalize
   operation_time_tests/comment_payout_dust
   operation_time_tests/recent_claims_decay
   operation_time_tests/vesting_withdrawals
   operation_time_tests/vesting_withdraw_route
   operation_time_tests/feed_publish_mean:103
   operation_time_tests/convert_delay
   operation_time_tests/hive_inflation
   operation_time_tests/hbd_interest
   operation_time_tests/hbd_savings_interest
   operation_time_tests/liquidity_rewards
   operation_time_tests/post_rate_limit
   operation_time_tests/comment_freeze
   operation_time_tests/hbd_stability:224
   operation_time_tests/hbd_price_feed_limit
   operation_time_tests/clear_null_account
   operation_time_tests/generate_account_subsidies
   operation_time_tests/account_subsidy_witness_limits
   operation_time_tests/recurrent_transfer_consecutive_failure_deletion:721
   operation_time_tests/recurrent_transfer_expiration:281
   operation_visitor/visit_performance:2726
   operation_visitor/bad_object
   proposal_tests/inactive_proposals_have_votes
   proposal_tests/db_remove_expired_governance_votes:222
   proposal_tests/db_remove_expired_governance_votes_with_proxy
   proposal_tests/db_remove_expired_governance_votes_threshold_exceeded
   proposal_tests/generating_payments
   proposal_tests/generating_payments_01
   proposal_tests/generating_payments_02
   proposal_tests/generating_payments_03
   proposal_tests/generating_payments_04
   proposal_tests/generating_payments_05
   proposal_tests/expired_proposals_forbidden_voting
   proposal_tests/proposals_maintenance
   proposal_tests/proposal_object_apply
   proposal_tests/proposal_object_apply_fee_increase
   proposal_tests/proposal_vote_object_apply
   proposal_tests/proposal_vote_object_01_apply
   proposal_tests/create_proposal_000
   proposal_tests/create_proposal_001
   proposal_tests/create_proposal_002
   proposal_tests/create_proposal_003
   proposal_tests/create_proposal_004
   proposal_tests/create_proposal_005
   proposal_tests/create_proposal_006
   proposal_tests/create_proposal_007
   proposal_tests/create_proposal_008
   proposal_tests/create_proposal_009
   proposal_tests/update_proposal_000
   proposal_tests/update_proposal_001
   proposal_tests/update_proposal_002
   proposal_tests/update_proposal_003
   proposal_tests/update_proposal_004
   proposal_tests/update_proposal_005
   proposal_tests/update_proposal_006
   proposal_tests/update_proposal_votes_000
   proposal_tests/update_proposal_votes_001
   proposal_tests/update_proposal_votes_002
   proposal_tests/update_proposal_votes_003
   proposal_tests/update_proposal_votes_004
   proposal_tests/update_proposal_votes_005
   proposal_tests/update_proposal_votes_006
   proposal_tests/remove_proposal_000
   proposal_tests/remove_proposal_001
   proposal_tests/remove_proposal_002
   proposal_tests/remove_proposal_003
   proposal_tests/remove_proposal_004
   proposal_tests/remove_proposal_005
   proposal_tests/remove_proposal_006
   proposal_tests/remove_proposal_007
   proposal_tests/remove_proposal_008
   proposal_tests/remove_proposal_009
   proposal_tests/remove_proposal_010
   proposal_tests/remove_proposal_011
   proposal_tests/remove_proposal_012
   proposal_tests/proposals_maintenance_01
   proposal_tests/proposals_maintenance_02
   proposal_tests/proposals_removing_with_threshold
   proposal_tests/proposals_removing_with_threshold_01
   proposal_tests/proposals_removing_with_threshold_02
   proposal_tests_performance/generating_payments:312
   proposal_tests_performance/converting_hive_to_dhf
   proposal_tests_performance/proposals_removing_with_threshold_03
   rc_tests/rc_usage_buckets
   rc_tests/rc_single_recover_account
   rc_tests/rc_many_recover_accounts
   rc_tests/rc_multisig_recover_account
   rc_tests/rc_tx_order_bug
   rc_tests/rc_pending_data_reset
   rc_tests/rc_differential_usage_operations
   rc_tests/rc_differential_usage_negative
   rc_tests/rc_differential_usage_many_ops
   rc_tests/rc_exception_during_modify
   serialization_tests/serialization_raw_test
   serialization_tests/serialization_json_test
   serialization_tests/legacy_asset_test
   serialization_tests/asset_test
   serialization_tests/asset_raw_test
   serialization_tests/json_tests
   serialization_tests/extended_private_key_type_test
   serialization_tests/extended_public_key_type_test
   serialization_tests/version_test
   serialization_tests/comment_options_extension_test
   serialization_tests/pow2_work_test
   serialization_tests/legacy_operation_test
   serialization_tests/block_header_test
   serialization_tests/hardfork_version_test
   serialization_tests/min_block_size
   serialization_tests/legacy_signed_transaction
   serialization_tests/static_variant_json_test
   serialization_tests/asset_symbol_type_test
   serialization_tests/unpack_clear_test
   serialization_tests/unpack_recursion_test
   serialization_tests/unpack_array_limit_test
   serialization_tests/incorrect_json_deserialization
   timeshare_tests/test_pre_hf26_behavior:123
   timeshare_tests/test_hf26_behavior:120
   timeshare_tests/test_hf27_behavior:120
   transaction_util_tests/autodetect_keys_simple
   transaction_util_tests/autodetect_keys_1_of_2
   transaction_util_tests/autodetect_keys_2_of_2
   transaction_util_tests/autodetect_keys_indirect_1_of_2
   transaction_util_tests/autodetect_keys_indirect_2_of_2
   transaction_util_tests/autodetect_keys_circular_and_deep
   transaction_util_tests/autodetect_keys_invalid
   tx_status_tests/regular_transactions
   tx_status_tests/popped_transactions
   tx_status_tests/transactions_in_forks
   tx_status_tests/failure_during_fork_switch
   undo_tests/undo_basic
   undo_tests/undo_object_disappear
   undo_tests/undo_key_collision
   undo_tests/undo_different_indexes
   undo_tests/undo_generate_blocks
   undo_tests/undo_delayed_votes
   undo_tests/undo_feed_history
   undo_tests/undo_state_on_squash_and_undo
   undo_tests/undo_state_on_push_and_commit
   undo_tests/undo_state_on_squash_and_undo_with_keep_alive
   undo_tests/empty_undo_benchmark
   undo_tests/debug_update_undo_bug
   undo_tests/filled_undo_benchmark
)

# needed to correctly print crash stacktrace
set_target_properties(chain_test PROPERTIES ENABLE_EXPORTS true)

target_link_libraries( chain_test db_fixture chainbase hive_chain hive_protocol account_history_rocksdb_plugin market_history_plugin witness_plugin debug_node_plugin fc ${PLATFORM_SPECIFIC_LIBS} )

file(GLOB PLUGIN_TESTS
      "plugin_tests/*.cpp"
)

parse_unit_tests(TEST_CASES ${PLUGIN_TESTS})

#message(STATUS "${TEST_CASES}")

add_boost_test( plugin_test
   SOURCES ${PLUGIN_TESTS}
   TESTS
    json_rpc/basic_validation
    json_rpc/syntax_validation
    json_rpc/misc_validation
    json_rpc/positive_validation
    json_rpc/semantics_validation
    market_history/mh_test:142
    transaction_status/transaction_status_test
    metadata_api_tests/get_account_metadata_test
    metadata_api_tests/find_account_metadata_test
    metadata_api_tests/account_create_metadata_test
)

# needed to correctly print crash stacktrace
set_target_properties(plugin_test PROPERTIES ENABLE_EXPORTS true)

target_link_libraries( plugin_test db_fixture hive_chain hive_protocol account_history_rocksdb_plugin market_history_plugin witness_plugin debug_node_plugin condenser_api_plugin database_api_plugin transaction_status_plugin transaction_status_api_plugin metadata_plugin metadata_api_plugin fc ${PLATFORM_SPECIFIC_LIBS} block_log_info_plugin hive_plugins )

if(MSVC)
  set_source_files_properties( tests/serialization_tests.cpp PROPERTIES COMPILE_FLAGS "/bigobj" )
endif(MSVC)

#add_subdirectory( generate_empty_blocks )
