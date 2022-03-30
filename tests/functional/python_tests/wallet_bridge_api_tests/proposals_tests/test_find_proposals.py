import pytest

import test_tools.exceptions

import local_tools

# TODO BUG LIST!
"""
1. Problem with calling wallet_bridge_api.find_proposals with the wrong data types in the arguments(# BUG1)
"""
ACCOUNTS = [f'account-{i}' for i in range(5)]


@pytest.mark.parametrize(
    'proposal_id', [
        [0],
        [0, 1, 2, 3, 4],
        [6],
    ]
)
def tests_with_correct_values(node, wallet, proposal_id):
    local_tools.create_accounts_with_vests_and_tbd(wallet, ACCOUNTS)
    local_tools.prepare_proposals(wallet, ACCOUNTS)

    node.api.wallet_bridge.find_proposals(proposal_id)


@pytest.mark.parametrize(
    'proposal_id', [
        -1,  # OUT OFF LIMITS: too low id
    ]
)
def tests_with_incorrect_values(node, wallet, proposal_id):
    local_tools.create_accounts_with_vests_and_tbd(wallet, ACCOUNTS)
    local_tools.prepare_proposals(wallet, ACCOUNTS)

    with pytest.raises(test_tools.exceptions.CommunicationError):
        node.api.wallet_bridge.find_proposals([proposal_id])


@pytest.mark.parametrize(
    'proposal_id', [
        'invalid-argument',
        # True,  # BUG1
    ]
)
def tests_with_incorrect_type_of_argument(node, proposal_id):
    with pytest.raises(test_tools.exceptions.CommunicationError):
        node.api.wallet_bridge.find_proposals([proposal_id])