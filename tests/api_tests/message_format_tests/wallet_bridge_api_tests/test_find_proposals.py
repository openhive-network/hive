import pytest

import test_tools as tt

from .local_tools import create_accounts_with_vests_and_tbd, prepare_proposals
from ..local_tools import as_string

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
def test_find_proposals_with_correct_values(node, wallet, proposal_ids):
    create_accounts_with_vests_and_tbd(wallet, ACCOUNTS)
    prepare_proposals(wallet, ACCOUNTS)

    node.api.wallet_bridge.find_proposals(proposal_ids)


@pytest.mark.parametrize(
    'proposal_id', [
        [-1],  # OUT OFF LIMITS: too low id
        ['true'],
    ]
)
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
def test_find_proposals_with_incorrect_type_of_argument(node, proposal_id):
    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.wallet_bridge.find_proposals(proposal_id)
