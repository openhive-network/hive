import pytest

import test_tools as tt

from .local_tools import as_string, create_accounts_with_vests_and_tbd, prepare_proposals, run_for


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
# proposals system was introduced after the 5 millionth block, it is only tested on node 64m
@run_for('testnet', 'mainnet_64m')
def test_find_proposals_with_correct_values(prepared_node, should_prepare, proposal_ids):
    if should_prepare:
        wallet = tt.Wallet(attach_to=prepared_node)
        create_accounts_with_vests_and_tbd(wallet, ACCOUNTS)
        prepare_proposals(wallet, ACCOUNTS)

    prepared_node.api.wallet_bridge.find_proposals(proposal_ids)


@pytest.mark.parametrize(
    'proposal_id', [
        [-1],  # OUT OFF LIMITS: too low id
        ['true'],
    ]
)
@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_find_proposals_with_incorrect_values(prepared_node, should_prepare, proposal_id):
    if should_prepare:
        wallet = tt.Wallet(attach_to=prepared_node)
        create_accounts_with_vests_and_tbd(wallet, ACCOUNTS)
        prepare_proposals(wallet, ACCOUNTS)

    with pytest.raises(tt.exceptions.CommunicationError):
        prepared_node.api.wallet_bridge.find_proposals(proposal_id)


@pytest.mark.parametrize(
    'proposal_id', [
        ['invalid-argument'],
        "[1,2,3,4,5]",
    ]
)
@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_find_proposals_with_incorrect_type_of_argument(prepared_node, proposal_id):
    with pytest.raises(tt.exceptions.CommunicationError):
        prepared_node.api.wallet_bridge.find_proposals(proposal_id)
