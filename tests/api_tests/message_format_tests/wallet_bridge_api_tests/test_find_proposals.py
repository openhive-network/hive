import pytest

import test_tools as tt

from .local_tools import as_string, create_accounts_with_vests_and_tbd, prepare_proposals
from ..local_tools import run_for


ACCOUNTS = [f'account-{i}' for i in range(5)]

CORRECT_VALUES = [
    [0],
    [0, 1, 2, 3, 4],
    [6],
]


@pytest.mark.parametrize(
    'proposal_ids', [
        *CORRECT_VALUES,
        *as_string(CORRECT_VALUES),
        [True],
    ]
)
@pytest.mark.testnet
def test_find_proposals_with_correct_values(node, wallet, proposal_ids):
    create_accounts_with_vests_and_tbd(wallet, ACCOUNTS)
    prepare_proposals(wallet, ACCOUNTS)

    node.api.wallet_bridge.find_proposals(proposal_ids)


#TODO SPRAWDZIĆ
@pytest.mark.remote_node_5m
def test_find_proposals_with_correct_values_5m(node5m):
    r = node5m.api.wallet_bridge.find_proposals([0])
    print()


#TODO SPRAWDZIĆ
@pytest.mark.remote_node_64m
def test_find_proposals_with_correct_values_64(node64m):
    node64m.api.wallet_bridge.find_proposals([0])


@pytest.mark.parametrize(
    'proposal_id', [
        [-1],  # OUT OFF LIMITS: too low id
        ['true'],
    ]
)
@pytest.mark.testnet
def test_find_proposals_with_incorrect_values(node, wallet, proposal_id):
    create_accounts_with_vests_and_tbd(wallet, ACCOUNTS)
    prepare_proposals(wallet, ACCOUNTS)

    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.wallet_bridge.find_proposals(proposal_id)


@pytest.mark.parametrize(
    'proposal_id', [
        ['invalid-argument'],
        "[1,2,3,4,5]",
    ]
)
@pytest.mark.testnet
def test_find_proposals_with_incorrect_type_of_argument(node, proposal_id):
    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.wallet_bridge.find_proposals(proposal_id)
