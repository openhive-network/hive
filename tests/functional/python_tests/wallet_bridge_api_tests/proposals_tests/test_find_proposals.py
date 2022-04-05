import json

import pytest

import test_tools.exceptions

import proposals_tools


ACCOUNTS = [f'account-{i}' for i in range(5)]


@pytest.mark.parametrize(
    'proposal_ids', [
        [0],
        [0, 1, 2, 3, 4],
        [6],
        [True],
    ]
)
def tests_with_correct_values(node, wallet, proposal_ids):
    proposals_tools.create_accounts_with_vests_and_tbd(wallet, ACCOUNTS)
    proposals_tools.prepare_proposals(wallet, ACCOUNTS)

    node.api.wallet_bridge.find_proposals(proposal_ids)


@pytest.mark.parametrize(
    'proposal_ids', [
        [0],
        [0, 1, 2, 3, 4],
        [6],
    ]
)
def tests_with_correct_values_as_strings(node, wallet, proposal_ids):
    for proposal_id_number in range(len(proposal_ids)):
        proposal_ids[proposal_id_number] = json.dumps(proposal_ids[proposal_id_number])

    proposals_tools.create_accounts_with_vests_and_tbd(wallet, ACCOUNTS)
    proposals_tools.prepare_proposals(wallet, ACCOUNTS)

    node.api.wallet_bridge.find_proposals(proposal_ids)


@pytest.mark.parametrize(
    'proposal_ids', [
        [0],
        [0, 1, 2, 3, 4],
        [6],
    ]
)
def test_list_of_arguments_as_string(node, wallet, proposal_ids):

    proposals_tools.create_accounts_with_vests_and_tbd(wallet, ACCOUNTS)
    proposals_tools.prepare_proposals(wallet, ACCOUNTS)

    with pytest.raises(test_tools.exceptions.CommunicationError):
        node.api.wallet_bridge.find_proposals(json.dumps(proposal_ids))


@pytest.mark.parametrize(
    'proposal_id', [
        [-1],  # OUT OFF LIMITS: too low id
        [json.dumps(True)],
    ]
)
def tests_with_incorrect_values(node, wallet, proposal_id):
    proposals_tools.create_accounts_with_vests_and_tbd(wallet, ACCOUNTS)
    proposals_tools.prepare_proposals(wallet, ACCOUNTS)

    with pytest.raises(test_tools.exceptions.CommunicationError):
        node.api.wallet_bridge.find_proposals(proposal_id)


@pytest.mark.parametrize(
    'proposal_id', [
        ['invalid-argument'],
    ]
)
def tests_with_incorrect_type_of_argument(node, proposal_id):
    with pytest.raises(test_tools.exceptions.CommunicationError):
        node.api.wallet_bridge.find_proposals(proposal_id)
