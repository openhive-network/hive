import json

import pytest

import test_tools.exceptions

import proposals_tools


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


@pytest.mark.parametrize(
    'start, limit, order_by, order_direction, status', [
        # START
            # by_voter_proposal
        ([''], 100, ORDER_BY['by_voter_proposal'], ORDER_DIRECTION['ascending'], STATUS['all']),

        ([10], 100, ORDER_BY['by_voter_proposal'], ORDER_DIRECTION['ascending'], STATUS['all']),

        (['non-exist-string'], 100, ORDER_BY['by_voter_proposal'], ORDER_DIRECTION['ascending'], STATUS['all']),

        ([True], 100, ORDER_BY['by_voter_proposal'], ORDER_DIRECTION['ascending'], STATUS['all']),

        ([ACCOUNTS[1]], 100, ORDER_BY['by_voter_proposal'], ORDER_DIRECTION['ascending'], STATUS['all']),

        # Start from nonexistent account (name "account-2a" is alphabetically between 'account-2' and 'account-3').
        (['account-2a'], 100, ORDER_BY['by_voter_proposal'], ORDER_DIRECTION['ascending'], STATUS['all']),

        ([ACCOUNTS[1], 3], 100, ORDER_BY['by_voter_proposal'], ORDER_DIRECTION['ascending'], STATUS['all']),

        ([ACCOUNTS[1], 3, 'additional_argument'], 100, ORDER_BY['by_voter_proposal'], ORDER_DIRECTION['ascending'],
         STATUS['all']),

            # by proposal voter
        ([''], 100, ORDER_BY['by_proposal_voter'], ORDER_DIRECTION['ascending'], STATUS['all']),

        ([3], 100, ORDER_BY['by_proposal_voter'], ORDER_DIRECTION['ascending'], STATUS['all']),

        ([3, ACCOUNTS[1]], 100, ORDER_BY['by_proposal_voter'], ORDER_DIRECTION['ascending'], STATUS['all']),

        ([3, ACCOUNTS[1], 'additional_argument'], 100, ORDER_BY['by_proposal_voter'], ORDER_DIRECTION['ascending'],
         STATUS['all']),

        # LIMIT
        ([''], 0, ORDER_BY['by_voter_proposal'], ORDER_DIRECTION['ascending'], STATUS['all']),
        ([''], 1000, ORDER_BY['by_voter_proposal'], ORDER_DIRECTION['ascending'], STATUS['all']),
        ([''], True, ORDER_BY['by_voter_proposal'], ORDER_DIRECTION['ascending'], STATUS['all']),

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
)
def tests_with_correct_values(node, wallet, start, limit, order_by, order_direction, status):
    proposals_tools.create_accounts_with_vests_and_tbd(wallet, ACCOUNTS)
    proposals_tools.prepare_proposals(wallet, ACCOUNTS)
    with wallet.in_single_transaction():
        for account in ACCOUNTS:
            wallet.api.update_proposal_votes(account, [3], 1)

    node.api.wallet_bridge.list_proposal_votes(start, limit, order_by, order_direction, status)


@pytest.mark.parametrize(
    'start, limit, order_by, order_direction, status', [
        # START
            # by_voter_proposal
        ([''], 100, ORDER_BY['by_voter_proposal'], ORDER_DIRECTION['ascending'], STATUS['all']),

        ([10], 100, ORDER_BY['by_voter_proposal'], ORDER_DIRECTION['ascending'], STATUS['all']),

        (['non-exist-string'], 100, ORDER_BY['by_voter_proposal'], ORDER_DIRECTION['ascending'], STATUS['all']),

        ([ACCOUNTS[1]], 100, ORDER_BY['by_voter_proposal'], ORDER_DIRECTION['ascending'], STATUS['all']),

        # Start from nonexistent account (name "account-2a" is alphabetically between 'account-2' and 'account-3').
        (['account-2a'], 100, ORDER_BY['by_voter_proposal'], ORDER_DIRECTION['ascending'], STATUS['all']),

        ([ACCOUNTS[1], 3], 100, ORDER_BY['by_voter_proposal'], ORDER_DIRECTION['ascending'], STATUS['all']),

        ([ACCOUNTS[1], 3, 'additional_argument'], 100, ORDER_BY['by_voter_proposal'], ORDER_DIRECTION['ascending'],
         STATUS['all']),

            # by proposal voter
        ([''], 100, ORDER_BY['by_proposal_voter'], ORDER_DIRECTION['ascending'], STATUS['all']),

        ([3], 100, ORDER_BY['by_proposal_voter'], ORDER_DIRECTION['ascending'], STATUS['all']),

        ([3, ACCOUNTS[1]], 100, ORDER_BY['by_proposal_voter'], ORDER_DIRECTION['ascending'], STATUS['all']),

        ([3, ACCOUNTS[1], 'additional_argument'], 100, ORDER_BY['by_proposal_voter'], ORDER_DIRECTION['ascending'],
         STATUS['all']),

        # LIMIT
        ([''], 0, ORDER_BY['by_voter_proposal'], ORDER_DIRECTION['ascending'], STATUS['all']),
        ([''], 1000, ORDER_BY['by_voter_proposal'], ORDER_DIRECTION['ascending'], STATUS['all']),

        # ORDER BY
        ([''], 100, ORDER_BY['by_voter_proposal'], ORDER_DIRECTION['ascending'], STATUS['all']),
        ([''], 100, ORDER_BY['by_proposal_voter'], ORDER_DIRECTION['ascending'], STATUS['all']),

        # ORDER DIRECTION
        ([''], 100, ORDER_BY['by_proposal_voter'], ORDER_DIRECTION['ascending'], STATUS['all']),
        ([''], 100, ORDER_BY['by_proposal_voter'], ORDER_DIRECTION['descending'], STATUS['all']),

        # STATUS
        ([''], 100, ORDER_BY['by_proposal_voter'], ORDER_DIRECTION['ascending'], STATUS['all']),
        ([''], 100, ORDER_BY['by_proposal_voter'], ORDER_DIRECTION['ascending'], STATUS['votable']),
    ]
)
def tests_with_correct_values_as_strings(node, wallet, start, limit, order_by, order_direction, status):
    proposals_tools.create_accounts_with_vests_and_tbd(wallet, ACCOUNTS)
    proposals_tools.prepare_proposals(wallet, ACCOUNTS)
    with wallet.in_single_transaction():
        for account in ACCOUNTS:
            wallet.api.update_proposal_votes(account, [3], 1)

    for start_number in range(len(start)):
        start[start_number] = json.dumps(start[start_number])

    limit = json.dumps(limit)
    order_by = json.dumps(order_by)
    order_direction = json.dumps(order_direction)
    status = json.dumps(status)

    node.api.wallet_bridge.list_proposal_votes(start, limit, order_by, order_direction, status)


@pytest.mark.parametrize(
    'start, limit, order_by, order_direction, status', [
        # START
        ([-2], 100, ORDER_BY['by_proposal_voter'], ORDER_DIRECTION['ascending'], STATUS['all']),
        ([json.dumps(True)], 100, ORDER_BY['by_proposal_voter'], ORDER_DIRECTION['ascending'], STATUS['all']),

        # LIMIT
        ([''], -1, ORDER_BY['by_voter_proposal'], ORDER_DIRECTION['ascending'], STATUS['all']),
        ([''], 1001, ORDER_BY['by_voter_proposal'], ORDER_DIRECTION['ascending'], STATUS['all']),
        ([''], json.dumps(True), ORDER_BY['by_voter_proposal'], ORDER_DIRECTION['ascending'], STATUS['all']),

        # ORDER BY
        ([''], 100, 32, ORDER_DIRECTION['ascending'], STATUS['all']),
        ([''], 100, 35, ORDER_DIRECTION['ascending'], STATUS['all']),
        ([''], 100, True, ORDER_DIRECTION['ascending'], STATUS['all']),
        ([''], 100, json.dumps(True), ORDER_DIRECTION['ascending'], STATUS['all']),

        # ORDER DIRECTION
        ([''], 100, ORDER_BY['by_voter_proposal'], -1, STATUS['all']),
        ([''], 100, ORDER_BY['by_voter_proposal'], 2, STATUS['all']),
        ([''], 100, ORDER_BY['by_voter_proposal'], json.dumps(True), STATUS['all']),

        # STATUS
        ([''], 100, ORDER_BY['by_voter_proposal'], ORDER_DIRECTION['ascending'], -1),
        ([''], 100, ORDER_BY['by_voter_proposal'], ORDER_DIRECTION['ascending'], 5),
        ([''], 100, ORDER_BY['by_voter_proposal'], ORDER_DIRECTION['ascending'], json.dumps(True)),
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
