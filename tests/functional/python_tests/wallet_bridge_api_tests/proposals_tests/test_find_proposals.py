import json

import pytest

import test_tools.exceptions

from . import local_tools


ACCOUNTS = [f'account-{i}' for i in range(5)]


CORRECT_VALUES = [
            [0],
            [0, 1, 2, 3, 4],
            [6],
        ]

@pytest.mark.parametrize(
    'proposal_ids', [
        *CORRECT_VALUES,
        *local_tools.as_string(CORRECT_VALUES),
        [True],
    ]
)
def test_find_proposals_with_correct_values(node, wallet, proposal_ids):
    local_tools.create_accounts_with_vests_and_tbd(wallet, ACCOUNTS)
    local_tools.prepare_proposals(wallet, ACCOUNTS)

    node.api.wallet_bridge.find_proposals(proposal_ids)


@pytest.mark.parametrize(
    'proposal_ids', [
        [0],
        [0, 1, 2, 3, 4],
        [6],
    ]
)

def test_find_proposals_with_list_of_arguments_as_string(node, wallet, proposal_ids):
    local_tools.create_accounts_with_vests_and_tbd(wallet, ACCOUNTS)
    local_tools.prepare_proposals(wallet, ACCOUNTS)

    with pytest.raises(test_tools.exceptions.CommunicationError):
        node.api.wallet_bridge.find_proposals(json.dumps(proposal_ids))


@pytest.mark.parametrize(
    'proposal_id', [
        [-1],  # OUT OFF LIMITS: too low id
        ['true'],
    ]
)
def test_find_proposals_with_incorrect_values(node, wallet, proposal_id):
    local_tools.create_accounts_with_vests_and_tbd(wallet, ACCOUNTS)
    local_tools.prepare_proposals(wallet, ACCOUNTS)

    with pytest.raises(test_tools.exceptions.CommunicationError):
        node.api.wallet_bridge.find_proposals(proposal_id)


@pytest.mark.parametrize(
    'proposal_id', [
        ['invalid-argument'],
    ]
)
def test_find_proposals_with_incorrect_type_of_argument(node, proposal_id):
    with pytest.raises(test_tools.exceptions.CommunicationError):
        node.api.wallet_bridge.find_proposals(proposal_id)
