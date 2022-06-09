import pytest

import test_tools as tt

from .local_tools import as_string, create_accounts_with_vests_and_tbd, date_from_now, prepare_proposals, run_for


ACCOUNTS = [f'account-{i}' for i in range(5)]

ORDER_BY = {
    'by_creator': 29,
    'by_start_date': 30,
    'by_end_date': 31,
    'by_total_votes': 32
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
        # At the moment there is an assumption, that no more than one start parameter is passed, more are ignored.
        # This is case, that recognize possibility of putting more than one argument in start.
        (['', 'example-string-argument', True, 123, 123], 100, ORDER_BY['by_creator'], ORDER_DIRECTION['ascending'], STATUS['all']),

            # by creator
        ([''], 100, ORDER_BY['by_creator'], ORDER_DIRECTION['ascending'], STATUS['all']),

        ([ACCOUNTS[1]], 100, ORDER_BY['by_creator'], ORDER_DIRECTION['ascending'], STATUS['all']),

        # Start from nonexistent account (name "account-2a" is alphabetically between 'account-2' and 'account-3').
        (['account-2a'], 100, ORDER_BY['by_creator'], ORDER_DIRECTION['ascending'], STATUS['all']),

        ([ACCOUNTS[1], ACCOUNTS[2]], 100, ORDER_BY['by_creator'], ORDER_DIRECTION['ascending'], STATUS['all']),

        ([10], 100, ORDER_BY['by_creator'], ORDER_DIRECTION['ascending'], STATUS['all']),

        ([True], 100, ORDER_BY['by_creator'], ORDER_DIRECTION['ascending'], STATUS['all']),

            # by start date
        ([], 100, ORDER_BY['by_start_date'], ORDER_DIRECTION['ascending'], STATUS['all']),

        ([date_from_now(weeks=20)], 100, ORDER_BY['by_start_date'], ORDER_DIRECTION['ascending'], STATUS['all']),

            # by end date
        ([], 100, ORDER_BY['by_end_date'], ORDER_DIRECTION['ascending'], STATUS['all']),

        ([date_from_now(weeks=20)], 100, ORDER_BY['by_end_date'], ORDER_DIRECTION['ascending'], STATUS['all']),

            # by total votes
        ([10], 100, ORDER_BY['by_total_votes'], ORDER_DIRECTION['ascending'], STATUS['all']),

        ([-10], 100, ORDER_BY['by_total_votes'], ORDER_DIRECTION['ascending'], STATUS['all']),

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


@pytest.mark.parametrize(
    'start, limit, order_by, order_direction, status', [
        *CORRECT_VALUES,
        *as_string(CORRECT_VALUES),
        ([True], 100, ORDER_BY['by_total_votes'], ORDER_DIRECTION['ascending'], STATUS['all']),
        ([''], True, ORDER_BY['by_creator'], ORDER_DIRECTION['ascending'], STATUS['all']),
        ([''], 100, ORDER_BY['by_creator'], True, STATUS['all']),
        ([''], 100, ORDER_BY['by_creator'], ORDER_DIRECTION['ascending'], True),
    ]
)
@run_for('testnet')
def test_list_proposals_with_correct_values_in_testnet(prepared_node, start, limit, order_by, order_direction, status):
    wallet = tt.Wallet(attach_to=prepared_node)
    create_accounts_with_vests_and_tbd(wallet, ACCOUNTS)
    prepare_proposals(wallet, ACCOUNTS)

    prepared_node.api.wallet_bridge.list_proposals(start, limit, order_by, order_direction, status)


@pytest.mark.parametrize(
    'start, limit, order_by, order_direction, status', [
        #START
            # by start date
        (['invalid-account-name'], 100, ORDER_BY['by_start_date'], ORDER_DIRECTION['ascending'], STATUS['all']),
        ([ACCOUNTS[1]], 100, ORDER_BY['by_start_date'], ORDER_DIRECTION['ascending'], STATUS['all']),
        (['true'], 100, ORDER_BY['by_start_date'], ORDER_DIRECTION['ascending'], STATUS['all']),
        ([True], 100, ORDER_BY['by_start_date'], ORDER_DIRECTION['ascending'], STATUS['all']),
        ([10], 100, ORDER_BY['by_start_date'], ORDER_DIRECTION['ascending'], STATUS['all']),

            # by end date
        (['invalid-account-name'], 100, ORDER_BY['by_end_date'], ORDER_DIRECTION['ascending'], STATUS['all']),
        ([ACCOUNTS[1]], 100, ORDER_BY['by_end_date'], ORDER_DIRECTION['ascending'], STATUS['all']),
        (['true'], 100, ORDER_BY['by_end_date'], ORDER_DIRECTION['ascending'], STATUS['all']),
        ([True], 100, ORDER_BY['by_end_date'], ORDER_DIRECTION['ascending'], STATUS['all']),
        ([10], 100, ORDER_BY['by_start_date'], ORDER_DIRECTION['ascending'], STATUS['all']),

            # by total votes
        (['invalid-account-name'], 100, ORDER_BY['by_total_votes'], ORDER_DIRECTION['ascending'], STATUS['all']),
        ([ACCOUNTS[1]], 100, ORDER_BY['by_total_votes'], ORDER_DIRECTION['ascending'], STATUS['all']),
        ([date_from_now(weeks=20)], 100, ORDER_BY['by_total_votes'], ORDER_DIRECTION['ascending'], STATUS['all']),
        (['true'], 100, ORDER_BY['by_total_votes'], ORDER_DIRECTION['ascending'], STATUS['all']),

        # LIMIT
        ([''], -1, ORDER_BY['by_creator'], ORDER_DIRECTION['ascending'], STATUS['all']),
        ([''], 1001, ORDER_BY['by_creator'], ORDER_DIRECTION['ascending'], STATUS['all']),
        ([''], 'true', ORDER_BY['by_creator'], ORDER_DIRECTION['ascending'], STATUS['all']),

        # ORDER BY
        ([''], 100, 28, ORDER_DIRECTION['ascending'], STATUS['all']),
        ([''], 100, 33, ORDER_DIRECTION['ascending'], STATUS['all']),
        ([''], 100, True, ORDER_DIRECTION['ascending'], STATUS['all']),
        ([''], 100, 'true', ORDER_DIRECTION['ascending'], STATUS['all']),

        # ORDER DIRECTION
        ([''], 100, ORDER_BY['by_creator'], -1, STATUS['all']),
        ([''], 100, ORDER_BY['by_creator'], 2, STATUS['all']),
        ([''], 100, ORDER_BY['by_creator'], 'true', STATUS['all']),

        # STATUS
        ([''], 100, ORDER_BY['by_creator'], ORDER_DIRECTION['ascending'], -1),
        ([''], 100, ORDER_BY['by_creator'], ORDER_DIRECTION['ascending'], 5),
        ([''], 100, ORDER_BY['by_creator'], ORDER_DIRECTION['ascending'], 'true'),
    ],
)
@run_for('testnet')
def test_list_proposals_with_incorrect_values(prepared_node, start, limit, order_by, order_direction, status):
    wallet = tt.Wallet(attach_to=prepared_node)
    create_accounts_with_vests_and_tbd(wallet, ACCOUNTS)
    prepare_proposals(wallet, ACCOUNTS)

    with pytest.raises(tt.exceptions.CommunicationError):
        prepared_node.api.wallet_bridge.list_proposals(start, limit, order_by, order_direction, status)


@pytest.mark.parametrize(
    'start, limit, order_by, order_direction, status', [
        # START
        (10, 100, 29, 0, 0),
        ('invalid-argument', 100, 29, 0, 0),
        (True, 100, 29, 0, 0),
        ([[]], 100, 29, 0, 0),
        ([{}], 100, 29, 0, 0),

        # LIMIT
        ([""], 'invalid-argument', 29, 0, 0),

        # ORDER TYPE
        ([""], 100, 'invalid-argument', 0, 0),

        # ORDER DIRECTION
        ([""], 100, 29, 'invalid-argument', 0),

        # STATUS
        ([""], 100, 29, 0, 'invalid-argument'),
    ]
)
@run_for('testnet')
def tests_list_proposals_with_incorrect_type_of_argument(prepared_node, start, limit, order_by, order_direction, status):
    with pytest.raises(tt.exceptions.CommunicationError):
        prepared_node.api.wallet_bridge.list_proposals(start, limit, order_by, order_direction, status)
