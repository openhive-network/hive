from datetime import datetime, timedelta

import pytest

import test_tools.exceptions

import local_tools

# TODO BUG LIST!
"""
1. Problem with calling wallet_bridge_api.list_proposals with the wrong data types in the arguments(# BUG1)
"""

ACCOUNTS = [f'account-{i}' for i in range(5)]

ORDER_BY = {
    'by_creator': 29,
    'by_start_date': 30,
    'by_end_date': 31
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


def date_from_now(*, weeks):
    future_data = datetime.now() + timedelta(weeks=weeks)
    return future_data.strftime('%Y-%m-%dT%H:%M:%S')


@pytest.mark.parametrize(
    'start, limit, order_by, order_direction, status', [
        # START
        # At the moment there is an assumption, that no more than one start parameter is passed, more are ignored

            # by creator
        ([''], 100, ORDER_BY['by_creator'], ORDER_DIRECTION['ascending'], STATUS['all']),
        ([ACCOUNTS[1]], 100, ORDER_BY['by_creator'], ORDER_DIRECTION['ascending'], STATUS['all']),
        # Start from nonexistent account (name "account-2a" is alphabetically between 'account-2' and 'account-3').
        (['account-2a'], 100, ORDER_BY['by_creator'], ORDER_DIRECTION['ascending'], STATUS['all']),
        ([ACCOUNTS[1], date_from_now(weeks=20)], 100, ORDER_BY['by_creator'],
         ORDER_DIRECTION['ascending'], STATUS['all']),
        ([ACCOUNTS[1], date_from_now(weeks=25)], 100, ORDER_BY['by_creator'],
         ORDER_DIRECTION['ascending'], STATUS['all']),
        ([ACCOUNTS[1], date_from_now(weeks=20), date_from_now(weeks=25)], 100, ORDER_BY['by_creator'],
         ORDER_DIRECTION['ascending'], STATUS['all']),
        ([ACCOUNTS[1], date_from_now(weeks=20), date_from_now(weeks=25), 'additional_argument'], 100,
         ORDER_BY['by_creator'], ORDER_DIRECTION['ascending'], STATUS['all']),

            # by start date
        ([], 100, 'by_start_date', 'ascending', 'all'),
        ([date_from_now(weeks=20)], 100, ORDER_BY['by_start_date'], ORDER_DIRECTION['ascending'],
         STATUS['all']),
        ([date_from_now(weeks=20), date_from_now(weeks=25)], 100, ORDER_BY['by_start_date'],
         ORDER_DIRECTION['ascending'], STATUS['all']),
        ([date_from_now(weeks=20), ACCOUNTS[1]], 100, ORDER_BY['by_start_date'],
         ORDER_DIRECTION['ascending'], STATUS['all']),
        ([date_from_now(weeks=20), ACCOUNTS[1], date_from_now(weeks=25)], 100, ORDER_BY['by_start_date'],
         ORDER_DIRECTION['ascending'], STATUS['all']),
        ([date_from_now(weeks=20), ACCOUNTS[1], date_from_now(weeks=25), 'additional_argument'], 100,
         ORDER_BY['by_start_date'], ORDER_DIRECTION['ascending'], STATUS['all']),

            # by end date
        ([], 100, ORDER_BY['by_end_date'], ORDER_DIRECTION['ascending'], STATUS['all']),
        ([date_from_now(weeks=25)], 100, ORDER_BY['by_end_date'], ORDER_DIRECTION['ascending'],
         STATUS['all']),
        ([date_from_now(weeks=25), date_from_now(weeks=20)], 100, ORDER_BY['by_end_date'],
         ORDER_DIRECTION['ascending'], STATUS['all']),
        ([date_from_now(weeks=25), ACCOUNTS[1]], 100, ORDER_BY['by_end_date'],
         ORDER_DIRECTION['ascending'], STATUS['all']),
        ([date_from_now(weeks=25), ACCOUNTS[1], date_from_now(weeks=25)], 100, ORDER_BY['by_end_date'],
         ORDER_DIRECTION['ascending'], STATUS['all']),
        ([date_from_now(weeks=25), ACCOUNTS[1], date_from_now(weeks=25), 'additional_argument'], 100,
         ORDER_BY['by_end_date'], ORDER_DIRECTION['ascending'], STATUS['all']),

        # LIMIT
        ([''], 0, ORDER_BY['by_creator'], ORDER_DIRECTION['ascending'], STATUS['all']),
        ([''], 1000, ORDER_BY['by_creator'], ORDER_DIRECTION['ascending'], STATUS['all']),

        # ORDER BY
        ([''], 100, ORDER_BY['by_creator'], ORDER_DIRECTION['ascending'], STATUS['all']),
        ([''], 100, ORDER_BY['by_end_date'], ORDER_DIRECTION['ascending'], STATUS['all']),

        # ORDER DIRECTION
        ([''], 100, ORDER_BY['by_creator'], ORDER_DIRECTION['ascending'], STATUS['all']),
        ([''], 100, ORDER_BY['by_creator'], ORDER_DIRECTION['descending'], STATUS['all']),

        # STATUS
        ([''], 100, ORDER_BY['by_creator'], ORDER_DIRECTION['ascending'], STATUS['all']),
        ([''], 100, ORDER_BY['by_creator'], ORDER_DIRECTION['ascending'], STATUS['votable']),
    ]
)
def tests_with_correct_values(node, wallet, start, limit, order_by, order_direction, status):
    local_tools.create_accounts_with_vests_and_tbd(wallet, ACCOUNTS)
    local_tools.prepare_proposals(wallet, ACCOUNTS)
    node.api.wallet_bridge.list_proposals(start, limit, order_by, order_direction, status)


@pytest.mark.parametrize(
    'start, limit, order_by, order_direction, status', [
        # LIMIT
        ([''], -1, ORDER_BY['by_creator'], ORDER_DIRECTION['ascending'], STATUS['all']),
        ([''], 1001, ORDER_BY['by_creator'], ORDER_DIRECTION['ascending'], STATUS['all']),

        # ORDER BY
        ([''], 100, 28, ORDER_DIRECTION['ascending'], STATUS['all']),
        ([''], 100, 32, ORDER_DIRECTION['ascending'], STATUS['all']),

        # ORDER DIRECTION
        ([''], 100, ORDER_BY['by_creator'], -1, STATUS['all']),
        ([''], 100, ORDER_BY['by_creator'], 2, STATUS['all']),

        # STATUS
        ([''], 100, ORDER_BY['by_creator'], ORDER_DIRECTION['ascending'], -1),
        ([''], 100, ORDER_BY['by_creator'], ORDER_DIRECTION['ascending'], 5),
    ],
)
def tests_with_incorrect_values(node, wallet, start, limit, order_by, order_direction, status):
    local_tools.create_accounts_with_vests_and_tbd(wallet, ACCOUNTS)
    local_tools.prepare_proposals(wallet, ACCOUNTS)
    with pytest.raises(test_tools.exceptions.CommunicationError):
        node.api.wallet_bridge.list_proposals(start, limit, order_by, order_direction, status)


@pytest.mark.parametrize(
    'start, limit, order_by, order_direction, status', [
        # START
        (10, 100, 29, 0, 0),
        ('invalid-argument', 100, 29, 0, 0),
        (True, 100, 29, 0, 0),

        # LIMIT
        ([""], 'invalid-argument', 29, 0, 0),
        # ([""], True, 29, 0, 0),  # BUG1

        # ORDER TYPE
        ([""], 100, 'invalid-argument', 0, 0),
        ([""], 100, True, 0, 0),

        # ORDER DIRECTION
        ([""], 100, 29, 'invalid-argument', 0),
        # ([""], 100, 29, True, 0),  # BUG1

        # STATUS
        ([""], 100, 29, 0, 'invalid-argument'),
        # ([""], 100, 29, 0, True),  # BUG1
    ]
)
def tests_with_incorrect_type_of_argument(node, start, limit, order_by, order_direction, status):
    with pytest.raises(test_tools.exceptions.CommunicationError):
        node.api.wallet_bridge.list_proposals(start, limit, order_by, order_direction, status)
