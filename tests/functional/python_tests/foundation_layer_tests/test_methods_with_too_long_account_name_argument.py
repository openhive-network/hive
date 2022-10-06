import pytest

import test_tools as tt

from ....local_tools import date_from_now

TOO_LONG_ACCOUNT_NAME = 'too-long-account-name'


@pytest.mark.parametrize(
    'api, method, arguments, keyword_arguments', [
        ('account_history', 'get_account_history', [], {'account': TOO_LONG_ACCOUNT_NAME, 'start': -1, 'limit': 100}),
        ('condenser', 'find_recurrent_transfers', (TOO_LONG_ACCOUNT_NAME,), {}),
        ('condenser', 'find_rc_accounts', ([TOO_LONG_ACCOUNT_NAME],), {}),
        ('condenser', 'get_account_history', (TOO_LONG_ACCOUNT_NAME, 0, 1), {}),
        ('condenser', 'get_account_reputations', (TOO_LONG_ACCOUNT_NAME, 100), {}),
        ('condenser', 'get_accounts', ([TOO_LONG_ACCOUNT_NAME],), {}),
        ('condenser', 'get_active_votes', (TOO_LONG_ACCOUNT_NAME, 'permlink-voted-post'), {}),
        ('condenser', 'get_collateralized_conversion_requests', (TOO_LONG_ACCOUNT_NAME,), {}),
        ('condenser', 'get_conversion_requests', (TOO_LONG_ACCOUNT_NAME,), {}),
        ('condenser', 'get_escrow', (TOO_LONG_ACCOUNT_NAME, 1), {}),
        ('condenser', 'get_expiring_vesting_delegations', (TOO_LONG_ACCOUNT_NAME, date_from_now(weeks=0)), {}),
        ('condenser', 'get_open_orders', (TOO_LONG_ACCOUNT_NAME,), {}),
        ('condenser', 'get_owner_history', (TOO_LONG_ACCOUNT_NAME,), {}),
        ('condenser', 'get_recovery_request', (TOO_LONG_ACCOUNT_NAME,), {}),
        ('condenser', 'get_reward_fund', (TOO_LONG_ACCOUNT_NAME,), {}),
        ('condenser', 'get_savings_withdraw_from', (TOO_LONG_ACCOUNT_NAME,), {}),
        ('condenser', 'get_savings_withdraw_to', (TOO_LONG_ACCOUNT_NAME,), {}),
        ('condenser', 'get_vesting_delegations', (TOO_LONG_ACCOUNT_NAME, TOO_LONG_ACCOUNT_NAME, 100), {}),
        ('condenser', 'get_withdraw_routes', (TOO_LONG_ACCOUNT_NAME, 'all'), {}),
        ('condenser', 'get_witness_by_account', (TOO_LONG_ACCOUNT_NAME,), {}),
        ('condenser', 'get_witnesses_by_vote', (TOO_LONG_ACCOUNT_NAME, 100), {}),
        ('condenser', 'list_proposal_votes', ([TOO_LONG_ACCOUNT_NAME], 100, 'by_voter_proposal', 'ascending', 'all'), {}),
        ('condenser', 'list_proposals', ([TOO_LONG_ACCOUNT_NAME], 100, 'by_creator', 'ascending', 'all'), {}),
        # TODO: Issue#389 After repair third argument problem uncomment this test
        # ('condenser', 'list_rc_accounts', (TOO_LONG_ACCOUNT_NAME, 100), {}),
        # TODO: Issue#389 After repair third argument problem uncomment this test
        # ('condenser', 'list_rc_direct_delegations', ([TOO_LONG_ACCOUNT_NAME, TOO_LONG_ACCOUNT_NAME], 100), {}),
        ('condenser', 'lookup_account_names', ([TOO_LONG_ACCOUNT_NAME],), {}),
        ('condenser', 'lookup_accounts', (TOO_LONG_ACCOUNT_NAME, 100), {}),
        ('condenser', 'lookup_witness_accounts', (TOO_LONG_ACCOUNT_NAME, 100), {}),
        ('condenser', 'verify_account_authority', (TOO_LONG_ACCOUNT_NAME, [tt.PublicKey(TOO_LONG_ACCOUNT_NAME)]), {}),
        ('database', 'list_witnesses', [], {'start': TOO_LONG_ACCOUNT_NAME, 'limit': 10, 'order': 'by_name'}),
        ('database', 'list_accounts', [], {'start': TOO_LONG_ACCOUNT_NAME, 'limit': 100, 'order': 'by_name'}),
        ('database', 'list_owner_histories', [], {'start': [TOO_LONG_ACCOUNT_NAME, '2022-04-11T10:29:00'], 'limit': 100}),
        ('database', 'list_account_recovery_requests', [], {'start': TOO_LONG_ACCOUNT_NAME, 'limit': 100, 'order': 'by_account'}),
        ('database', 'list_change_recovery_account_requests', [], {'start': TOO_LONG_ACCOUNT_NAME, 'limit': 100, 'order': 'by_account'}),
        ('database', 'list_escrows', [], {'start': [TOO_LONG_ACCOUNT_NAME, 0], 'limit': 100, 'order': 'by_from_id'}),
        ('database', 'list_withdraw_vesting_routes', [], {'start': [TOO_LONG_ACCOUNT_NAME, TOO_LONG_ACCOUNT_NAME], 'limit': 100, 'order': 'by_withdraw_route'}),
        ('database', 'list_savings_withdrawals', [], {'start': ['2022-04-11T10:29:00', TOO_LONG_ACCOUNT_NAME, 0], 'limit': 100, 'order': 'by_complete_from_id'}),
        ('database', 'list_vesting_delegations', [], {'start': [TOO_LONG_ACCOUNT_NAME, TOO_LONG_ACCOUNT_NAME], 'limit': 100, 'order': 'by_delegation'}),
        ('database', 'list_vesting_delegation_expirations', [], {'start': [TOO_LONG_ACCOUNT_NAME, '2022-04-11T10:29:00', 0], 'limit': 100, 'order': 'by_account_expiration'}),
        ('database', 'list_hbd_conversion_requests', [], {'start': [TOO_LONG_ACCOUNT_NAME, 0], 'limit': 100, 'order': 'by_account'}),
        ('database', 'list_collateralized_conversion_requests', [], {'start': [TOO_LONG_ACCOUNT_NAME], 'limit': 100, 'order': 'by_account'}),
        ('database', 'list_decline_voting_rights_requests', [], {'start': TOO_LONG_ACCOUNT_NAME, 'limit': 100, 'order': 'by_account'}),
        ('database', 'list_limit_orders', [], {'start': [TOO_LONG_ACCOUNT_NAME, 0], 'limit': 100, 'order': 'by_account'}),
        ('database', 'list_proposals', [], {'start': [TOO_LONG_ACCOUNT_NAME], 'limit': 100, 'order': 'by_creator', 'order_direction': 'ascending', 'status': 'all'}),
        ('database', 'list_proposal_votes', [], {'start': [TOO_LONG_ACCOUNT_NAME], 'limit': 100, 'order': 'by_voter_proposal', 'order_direction': 'ascending', 'status': 'all'}),
        ('database', 'list_witness_votes', [], {'start': [TOO_LONG_ACCOUNT_NAME, TOO_LONG_ACCOUNT_NAME], 'limit': 100, 'order': 'by_witness_account'}),
        ('database', 'find_account_recovery_requests', [], {'accounts': [TOO_LONG_ACCOUNT_NAME, TOO_LONG_ACCOUNT_NAME]}),
        ('database', 'find_accounts', [], {'accounts': [TOO_LONG_ACCOUNT_NAME, TOO_LONG_ACCOUNT_NAME]}),
        ('database', 'find_change_recovery_account_requests', [], {'accounts': [TOO_LONG_ACCOUNT_NAME, TOO_LONG_ACCOUNT_NAME]}),
        ('database', 'find_collateralized_conversion_requests', [], {'account': TOO_LONG_ACCOUNT_NAME}),
        ('database', 'find_comments', [], {'comments': [[TOO_LONG_ACCOUNT_NAME, 'https://api.hive.blog']]}),
        ('database', 'find_decline_voting_rights_requests', [], {'accounts': [TOO_LONG_ACCOUNT_NAME, TOO_LONG_ACCOUNT_NAME]}),
        ('database', 'find_escrows', [], {'from': TOO_LONG_ACCOUNT_NAME}),
        ('database', 'find_hbd_conversion_requests', [], {'account': TOO_LONG_ACCOUNT_NAME}),
        ('database', 'find_limit_orders', [], {'account': TOO_LONG_ACCOUNT_NAME}),
        ('database', 'find_owner_histories', [], {'owner': TOO_LONG_ACCOUNT_NAME}),
        ('database', 'find_recurrent_transfers', [], {'from': TOO_LONG_ACCOUNT_NAME}),
        ('database', 'find_savings_withdrawals', [], {'account': TOO_LONG_ACCOUNT_NAME}),
        ('database', 'find_vesting_delegation_expirations', [], {'account': TOO_LONG_ACCOUNT_NAME}),
        ('database', 'find_vesting_delegations', [], {'account': TOO_LONG_ACCOUNT_NAME}),
        ('database', 'find_withdraw_vesting_routes', [], {'account': TOO_LONG_ACCOUNT_NAME, 'order': 'by_destination'}),
        ('database', 'find_witnesses', [], {'owners': [TOO_LONG_ACCOUNT_NAME]}),
        ('database', 'get_comment_pending_payouts', [], {'comments': [[TOO_LONG_ACCOUNT_NAME, TOO_LONG_ACCOUNT_NAME]]}),
        ('reputation', 'get_account_reputations', [], {'account_lower_bound': TOO_LONG_ACCOUNT_NAME, 'limit': 1000}),
        ('rc', 'find_rc_accounts', [], {'accounts': [TOO_LONG_ACCOUNT_NAME]}),
        ('wallet_bridge', 'find_rc_accounts', ([TOO_LONG_ACCOUNT_NAME, TOO_LONG_ACCOUNT_NAME],), {}),
        ('wallet_bridge', 'find_recurrent_transfers', (TOO_LONG_ACCOUNT_NAME,), {}),
        ('wallet_bridge', 'get_account', (TOO_LONG_ACCOUNT_NAME,), {}),
        ('wallet_bridge', 'get_account_history', (TOO_LONG_ACCOUNT_NAME, 0, 1), {}),
        ('wallet_bridge', 'get_accounts', ([TOO_LONG_ACCOUNT_NAME],), {}),
        ('wallet_bridge', 'get_collateralized_conversion_requests', (TOO_LONG_ACCOUNT_NAME,), {}),
        ('wallet_bridge', 'get_conversion_requests', (TOO_LONG_ACCOUNT_NAME,), {}),
        ('wallet_bridge', 'get_open_orders', (TOO_LONG_ACCOUNT_NAME,), {}),
        ('wallet_bridge', 'get_owner_history', (TOO_LONG_ACCOUNT_NAME,), {}),
        ('wallet_bridge', 'get_reward_fund', (TOO_LONG_ACCOUNT_NAME,), {}),
        ('wallet_bridge', 'get_withdraw_routes', (TOO_LONG_ACCOUNT_NAME, 'all'), {}),
        ('wallet_bridge', 'get_witness', (TOO_LONG_ACCOUNT_NAME,), {}),
        ('wallet_bridge', 'list_accounts', (TOO_LONG_ACCOUNT_NAME, 100), {}),
        ('wallet_bridge', 'list_proposal_votes', ([TOO_LONG_ACCOUNT_NAME], 100, 'by_voter_proposal', 'ascending', 'all'), {}),
        ('wallet_bridge', 'list_proposals', ([TOO_LONG_ACCOUNT_NAME], 100, 'by_creator', 'ascending', 'active'), {}),
        ('wallet_bridge', 'list_rc_accounts', (TOO_LONG_ACCOUNT_NAME, 100), {}),
        ('wallet_bridge', 'list_rc_direct_delegations', ([TOO_LONG_ACCOUNT_NAME, TOO_LONG_ACCOUNT_NAME], 100), {}),
        ('wallet_bridge', 'list_witnesses', (TOO_LONG_ACCOUNT_NAME, 10, 'by_name'), {}),
    ]
)
@pytest.mark.testnet
def test_methods_with_too_long_account_name_argument(node, api, method, arguments, keyword_arguments):
    def call(node_, api_, method_, *arguments_, **keyword_arguments_):
        selected_api = getattr(node_.api, api_)
        selected_method = getattr(selected_api, method_)
        return selected_method(*arguments_, **keyword_arguments_)

    with pytest.raises(tt.exceptions.CommunicationError) as exception:
        call(node, api, method, *arguments, **keyword_arguments)

    response = exception.value.response
    assert (
        f"Assert Exception:in_len <= sizeof(data): Input too large: `{TOO_LONG_ACCOUNT_NAME}'"
        f" ({len(TOO_LONG_ACCOUNT_NAME)}) for fixed size string: 16"
    ) == response['error']['message']
