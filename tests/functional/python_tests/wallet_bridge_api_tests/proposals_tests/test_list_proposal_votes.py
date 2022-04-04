import pytest

import test_tools.exceptions

import proposals_tools

# TODO BUG LIST!
"""
1. Problem with calling wallet_bridge_api.list_proposal_votes with the wrong data types in the arguments(# BUG1) [SOLVED!]

2. Problem with calling wallet_bridge_api.list_proposal_votes with the wrong data types in argument START(# BUG2) [SOLVED!]

3. Problem with calling wallet_bridge_api.list_proposal_votes with argument "['True']"
     Sent: {"jsonrpc": "2.0", "id": 1, "method": "wallet_bridge_api.list_proposal_votes", "params": [[[""], "True", "33", "0", "0"]]}'
     Received: 'message': "Parse Error:Couldn't parse int64_t"
     (# BUG3)  [SOLVED!]
"""
ACCOUNTS = [f'account-{i}' for i in range(5)]

ORDER_BY = {
    'by_voter_proposal': 33,
    'by_proposal_voter': 34,
}

ORDER_DIRECTION = {
    'ascending': 0,
    'descending': 1
}

STATUS = {
    'all': 0,
    'inactive': 1,
    'active': 2,
    'expired': 3,
    'votable': 4
}

CORRECT_VALUES = [
        # START

            # by_voter_proposal
        ([''], 100, ORDER_BY['by_voter_proposal'], ORDER_DIRECTION['ascending'], STATUS['all']),
        ([10], 100, ORDER_BY['by_voter_proposal'], ORDER_DIRECTION['ascending'], STATUS['all']),
        (['non-exist-string'], 100, ORDER_BY['by_voter_proposal'], ORDER_DIRECTION['ascending'], STATUS['all']),
        ([True], 100, ORDER_BY['by_voter_proposal'], ORDER_DIRECTION['ascending'], STATUS['all']),
        ([ACCOUNTS[1]], 100, ORDER_BY['by_voter_proposal'], ORDER_DIRECTION['ascending'],
         STATUS['all']),
        # Start from nonexistent account (name "account-2a" is alphabetically between 'account-2' and 'account-3').
        (['account-2a'], 100, ORDER_BY['by_voter_proposal'], ORDER_DIRECTION['ascending'],
         STATUS['all']),
        ([ACCOUNTS[1], 3], 100, ORDER_BY['by_voter_proposal'], ORDER_DIRECTION['ascending'],
         STATUS['all']),
        ([ACCOUNTS[1], 3, 'additional_argument'], 100, ORDER_BY['by_voter_proposal'],
         ORDER_DIRECTION['ascending'], STATUS['all']),

            # by proposal voter
        ([''], 100, ORDER_BY['by_proposal_voter'], ORDER_DIRECTION['ascending'], STATUS['all']),
        ([3], 100, ORDER_BY['by_proposal_voter'], ORDER_DIRECTION['ascending'], STATUS['all']),
        ([3, ACCOUNTS[1]], 100, ORDER_BY['by_proposal_voter'], ORDER_DIRECTION['ascending'],
         STATUS['all']),
        ([3, ACCOUNTS[1], 'additional_argument'], 100, ORDER_BY['by_proposal_voter'],
         ORDER_DIRECTION['ascending'], STATUS['all']),

        # LIMIT
        ([''], 0, ORDER_BY['by_voter_proposal'], ORDER_DIRECTION['ascending'], STATUS['all']),
        ([''], 1000, ORDER_BY['by_voter_proposal'], ORDER_DIRECTION['ascending'], STATUS['all']),
        ([''], True, ORDER_BY['by_voter_proposal'], ORDER_DIRECTION['ascending'], STATUS['all']),  # BUG3

        # ORDER BY
        ([''], 100, ORDER_BY['by_voter_proposal'], ORDER_DIRECTION['ascending'], STATUS['all']),
        ([''], 100, ORDER_BY['by_proposal_voter'], ORDER_DIRECTION['ascending'], STATUS['all']),

        # ORDER DIRECTION
        ([''], 100, ORDER_BY['by_proposal_voter'], ORDER_DIRECTION['ascending'], STATUS['all']),
        ([''], 100, ORDER_BY['by_proposal_voter'], ORDER_DIRECTION['descending'], STATUS['all']),
        ([''], 100, ORDER_BY['by_proposal_voter'], True, STATUS['all']),

        # STATUS
        ([''], 100, ORDER_BY['by_proposal_voter'], ORDER_DIRECTION['ascending'], STATUS['all']),
        ([''], 100, ORDER_BY['by_proposal_voter'], ORDER_DIRECTION['ascending'], STATUS['votable']),
        ([''], 100, ORDER_BY['by_proposal_voter'], ORDER_DIRECTION['ascending'], True),
]

@pytest.mark.parametrize(
    'start, limit, order_by, order_direction, status', CORRECT_VALUES,
)
def tests_with_correct_values(node, wallet, start, limit, order_by, order_direction, status):
    proposals_tools.create_accounts_with_vests_and_tbd(wallet, ACCOUNTS)
    proposals_tools.prepare_proposals(wallet, ACCOUNTS)
    with wallet.in_single_transaction():
        for account in ACCOUNTS:
            wallet.api.update_proposal_votes(account, [3], 1)

    for start_number in range(len(start)):
        start[start_number] = proposals_tools.add_quotes_to_bool_or_numeric(start[start_number])
    limit = proposals_tools.add_quotes_to_bool_or_numeric(limit)
    order_by = proposals_tools.add_quotes_to_bool_or_numeric(order_by)
    order_direction = proposals_tools.add_quotes_to_bool_or_numeric(order_direction)
    status = proposals_tools.add_quotes_to_bool_or_numeric(status)


@pytest.mark.parametrize(
    'start, limit, order_by, order_direction, status', CORRECT_VALUES,
)
def tests_with_correct_values_in_quotes(node, wallet, start, limit, order_by, order_direction, status):
    proposals_tools.create_accounts_with_vests_and_tbd(wallet, ACCOUNTS)
    proposals_tools.prepare_proposals(wallet, ACCOUNTS)
    with wallet.in_single_transaction():
        for account in ACCOUNTS:
            wallet.api.update_proposal_votes(account, [3], 1)

    node.api.wallet_bridge.list_proposal_votes(start, limit, order_by, order_direction, status)


    if limit == 'True' or order_direction == 'True' or status == 'True':   # Bool in quotes have special work and not throw exception
        with pytest.raises(test_tools.exceptions.CommunicationError):
            node.api.wallet_bridge.list_proposal_votes(start, limit, order_by, order_direction, status)
    else:
        node.api.wallet_bridge.list_proposal_votes(start, limit, order_by, order_direction, status)


@pytest.mark.parametrize(
    'start, limit, order_by, order_direction, status', [
        # START
        ([-2], 100, ORDER_BY['by_proposal_voter'], ORDER_DIRECTION['ascending'], STATUS['all']),

        # LIMIT
        ([''], -1, ORDER_BY['by_voter_proposal'], ORDER_DIRECTION['ascending'], STATUS['all']),
        ([''], 1001, ORDER_BY['by_voter_proposal'], ORDER_DIRECTION['ascending'], STATUS['all']),

        # ORDER BY
        ([''], 100, 32, ORDER_DIRECTION['ascending'], STATUS['all']),
        ([''], 100, 35, ORDER_DIRECTION['ascending'], STATUS['all']),
        ([''], 100, True, ORDER_DIRECTION['ascending'], STATUS['all']),

        # ORDER DIRECTION
        ([''], 100, ORDER_BY['by_voter_proposal'], -1, STATUS['all']),
        ([''], 100, ORDER_BY['by_voter_proposal'], 2, STATUS['all']),

        # STATUS
        ([''], 100, ORDER_BY['by_voter_proposal'], ORDER_DIRECTION['ascending'], -1),
        ([''], 100, ORDER_BY['by_voter_proposal'], ORDER_DIRECTION['ascending'], 5),
    ],
)
def tests_with_incorrect_values(node, wallet, start, limit, order_by, order_direction, status):
    proposals_tools.create_accounts_with_vests_and_tbd(wallet, ACCOUNTS)
    proposals_tools.prepare_proposals(wallet, ACCOUNTS)
    with wallet.in_single_transaction():
        for account in ACCOUNTS:
            wallet.api.update_proposal_votes(account, [3], 1)

    with pytest.raises(test_tools.exceptions.CommunicationError):
        node.api.wallet_bridge.list_proposal_votes(start, limit, order_by, order_direction, status)


@pytest.mark.parametrize(
    'start, limit, order_by, order_direction, status', [
        # LIMIT
        ([""], 'invalid-argument', 33, 0, 0),

        # ORDER TYPE
        ([""], 100, 'invalid-argument', 0, 0),

        # ORDER DIRECTION
        ([""], 100, 33, 'invalid-argument', 0),

        # STATUS
        ([""], 100, 33, 0, 'invalid-argument'),
    ]
)
def tests_with_incorrect_type_of_argument(node, start, limit, order_by, order_direction, status):
    with pytest.raises(test_tools.exceptions.CommunicationError):
        node.api.wallet_bridge.list_proposal_votes(start, limit, order_by, order_direction, status)
