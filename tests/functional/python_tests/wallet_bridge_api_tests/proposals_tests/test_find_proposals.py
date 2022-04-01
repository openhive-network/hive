import pytest

import test_tools.exceptions

import proposals_tools

# TODO BUG LIST!
"""
1. Problem with calling wallet_bridge_api.find_proposals with the wrong data types in the arguments(# BUG1) [SOLVED!]
2. Problem with calling wallet_bridge_api.find_proposals with argument "['True']"
     Sent: {"jsonrpc": "2.0", "id": 1, "method": "wallet_bridge_api.find_proposals", "params": [["True"]]}'
     Received: 'message': "Parse Error:Couldn't parse int64_t"
     (# BUG2)  [SOLVED!]
"""

ACCOUNTS = [f'account-{i}' for i in range(5)]

CORRECT_VALUES = [
    [0],
    [0, 1, 2, 3, 4],
    [6],
    [True],  # BUG2
]


@pytest.mark.parametrize(
    'proposal_ids', CORRECT_VALUES
)
def tests_with_correct_values(node, wallet, proposal_ids):
    proposals_tools.create_accounts_with_vests_and_tbd(wallet, ACCOUNTS)
    proposals_tools.prepare_proposals(wallet, ACCOUNTS)

    node.api.wallet_bridge.find_proposals(proposal_ids)


@pytest.mark.parametrize(
    'proposal_ids', CORRECT_VALUES
)
def tests_with_correct_values_in_quotes(node, wallet, proposal_ids):
    for proposal_id_number in range(len(proposal_ids)):
        proposal_ids[proposal_id_number] = proposals_tools.add_quotes_to_bool_or_numeric(proposal_ids[proposal_id_number])

    proposals_tools.create_accounts_with_vests_and_tbd(wallet, ACCOUNTS)
    proposals_tools.prepare_proposals(wallet, ACCOUNTS)

    if proposal_ids == ['True']:   # Bool in quotes have special work and not throw exception
        with pytest.raises(test_tools.exceptions.CommunicationError):
            node.api.wallet_bridge.find_proposals(proposal_ids)
    else:
        node.api.wallet_bridge.find_proposals(proposal_ids)


@pytest.mark.parametrize(
    'proposal_id', [
        -1,  # OUT OFF LIMITS: too low id
    ]
)
def tests_with_incorrect_values(node, wallet, proposal_id):
    proposals_tools.create_accounts_with_vests_and_tbd(wallet, ACCOUNTS)
    proposals_tools.prepare_proposals(wallet, ACCOUNTS)

    with pytest.raises(test_tools.exceptions.CommunicationError):
        node.api.wallet_bridge.find_proposals([proposal_id])


@pytest.mark.parametrize(
    'proposal_id', [
        'invalid-argument',
    ]
)
def tests_with_incorrect_type_of_argument(node, proposal_id):
    with pytest.raises(test_tools.exceptions.CommunicationError):
        node.api.wallet_bridge.find_proposals([proposal_id])
