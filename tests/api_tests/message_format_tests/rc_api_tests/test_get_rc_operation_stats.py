import pytest
import test_tools as tt

INCORRECT_OPERATION_NAME = 'nonexistent_operation'
INCORRECT_OPERATION_INDEXES = [-1, 51]
LIST_OF_OPERATIONS = [
    ('vote_operation', 0),
    ('comment_operation', 1),
    ('transfer_operation', 2),
    ('transfer_to_vesting_operation', 3),
    ('withdraw_vesting_operation', 4),
    ('limit_order_create_operation', 5),
    ('limit_order_cancel_operation', 6),
    ('feed_publish_operation', 7),
    ('convert_operation', 8),
    ('account_create_operation', 9),
    ('account_update_operation', 10),
    ('witness_update_operation', 11),
    ('account_witness_vote_operation', 12),
    ('account_witness_proxy_operation', 13),
    ('pow_operation', 14),
    ('custom_operation', 15),
    ('witness_block_approve_operation', 16),
    ('delete_comment_operation', 17),
    ('custom_json_operation', 18),
    ('comment_options_operation', 19),
    ('set_withdraw_vesting_route_operation', 20),
    ('limit_order_create2_operation', 21),
    ('claim_account_operation', 22),
    ('create_claimed_account_operation', 23),
    ('request_account_recovery_operation', 24),
    ('recover_account_operation', 25),
    ('change_recovery_account_operation', 26),
    ('escrow_transfer_operation', 27),
    ('escrow_dispute_operation', 28),
    ('escrow_release_operation', 29),
    ('pow2_operation', 30),
    ('escrow_approve_operation', 31),
    ('transfer_to_savings_operation', 32),
    ('transfer_from_savings_operation', 33),
    ('cancel_transfer_from_savings_operation', 34),
    ('custom_binary_operation', 35),
    ('decline_voting_rights_operation', 36),
    ('reset_account_operation', 37),
    ('set_reset_account_operation', 38),
    ('claim_reward_balance_operation', 39),
    ('delegate_vesting_shares_operation', 40),
    ('account_create_with_delegation_operation', 41),
    ('witness_set_properties_operation', 42),
    ('account_update2_operation', 43),
    ('create_proposal_operation', 44),
    ('update_proposal_votes_operation', 45),
    ('remove_proposal_operation', 46),
    ('update_proposal_operation', 47),
    ('collateralized_convert_operation', 48),
    ('recurrent_transfer_operation', 49),
    ('multiop', 50),
]


@pytest.mark.parametrize(
    'operation', [op for op, op_index in LIST_OF_OPERATIONS]
)
def test_get_rc_operation_stats_with_valid_operation_name(remote_node, operation):
    response = remote_node.api.rc.get_rc_operation_stats(operation=operation)


@pytest.mark.parametrize(
    'operation_index', [op_index for op, op_index in LIST_OF_OPERATIONS]
)
def test_get_rc_operation_stats_with_valid_operation_number(remote_node, operation_index):
    response = remote_node.api.rc.get_rc_operation_stats(operation=operation_index)


def test_get_rc_operation_stats_with_nonexistent_operation_name(remote_node):
    with pytest.raises(tt.exceptions.CommunicationError):
        remote_node.api.rc.get_rc_operation_stats(operation=INCORRECT_OPERATION_NAME)


@pytest.mark.parametrize(
    'invalid_operation_index', [INCORRECT_OPERATION_INDEXES[0], INCORRECT_OPERATION_INDEXES[1]]
)
def test_get_rc_operation_stats_with_nonexistent_operation_index(remote_node, invalid_operation_index):
    with pytest.raises(tt.exceptions.CommunicationError):
        remote_node.api.rc.get_rc_operation_stats(operation=invalid_operation_index)
