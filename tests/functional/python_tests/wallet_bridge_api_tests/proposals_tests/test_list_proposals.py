import pytest

import test_tools.exceptions

<<<<<<< HEAD
import proposals_tools
=======
from .local_tools import as_string, create_accounts_with_vests_and_tbd, date_from_now, prepare_proposals
>>>>>>> 8bf85f216 (fixup! Rebuild test_list_proposal_votes.py)


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

        ([ACCOUNTS[1], date_from_now(weeks=20)], 100, ORDER_BY['by_creator'], ORDER_DIRECTION['ascending'],
         STATUS['all']),

        ([ACCOUNTS[1], date_from_now(weeks=25)], 100, ORDER_BY['by_creator'], ORDER_DIRECTION['ascending'],
         STATUS['all']),

        ([ACCOUNTS[1], date_from_now(weeks=20), date_from_now(weeks=25)], 100, ORDER_BY['by_creator'],
         ORDER_DIRECTION['ascending'], STATUS['all']),

        ([ACCOUNTS[1], date_from_now(weeks=20), date_from_now(weeks=25), 'additional_argument'], 100,
         ORDER_BY['by_creator'], ORDER_DIRECTION['ascending'], STATUS['all']),

            # by start date
        ([], 100, ORDER_BY['by_start_date'], ORDER_DIRECTION['ascending'], STATUS['all']),

        ([date_from_now(weeks=20)], 100, ORDER_BY['by_start_date'], ORDER_DIRECTION['ascending'], STATUS['all']),

        ([date_from_now(weeks=20), date_from_now(weeks=25)], 100, ORDER_BY['by_start_date'],
         ORDER_DIRECTION['ascending'], STATUS['all']),

        ([date_from_now(weeks=20), ACCOUNTS[1]], 100, ORDER_BY['by_start_date'], ORDER_DIRECTION['ascending'],
         STATUS['all']),

        ([date_from_now(weeks=20), ACCOUNTS[1], date_from_now(weeks=25)], 100, ORDER_BY['by_start_date'],
         ORDER_DIRECTION['ascending'], STATUS['all']),

        ([date_from_now(weeks=20), ACCOUNTS[1], date_from_now(weeks=25), 'additional_argument'], 100,
         ORDER_BY['by_start_date'], ORDER_DIRECTION['ascending'], STATUS['all']),

            # by end date
        ([], 100, ORDER_BY['by_end_date'], ORDER_DIRECTION['ascending'], STATUS['all']),

        ([date_from_now(weeks=25)], 100, ORDER_BY['by_end_date'], ORDER_DIRECTION['ascending'], STATUS['all']),

        ([date_from_now(weeks=25), date_from_now(weeks=20)], 100, ORDER_BY['by_end_date'], ORDER_DIRECTION['ascending'],
         STATUS['all']),

        ([date_from_now(weeks=25), ACCOUNTS[1]], 100, ORDER_BY['by_end_date'], ORDER_DIRECTION['ascending'],
         STATUS['all']),

        ([date_from_now(weeks=25), ACCOUNTS[1], date_from_now(weeks=25)], 100, ORDER_BY['by_end_date'],
         ORDER_DIRECTION['ascending'], STATUS['all']),

        ([date_from_now(weeks=25), ACCOUNTS[1], date_from_now(weeks=25), 'additional_argument'], 100,
         ORDER_BY['by_end_date'], ORDER_DIRECTION['ascending'], STATUS['all']),

        # LIMIT
        ([''], 0, ORDER_BY['by_creator'], ORDER_DIRECTION['ascending'], STATUS['all']),
        ([''], 1000, ORDER_BY['by_creator'], ORDER_DIRECTION['ascending'], STATUS['all']),
        ([''], True, ORDER_BY['by_creator'], ORDER_DIRECTION['ascending'], STATUS['all']),

        # ORDER BY
        ([''], 100, ORDER_BY['by_creator'], ORDER_DIRECTION['ascending'], STATUS['all']),
        ([''], 100, ORDER_BY['by_end_date'], ORDER_DIRECTION['ascending'], STATUS['all']),

        # ORDER DIRECTION
        ([''], 100, ORDER_BY['by_creator'], ORDER_DIRECTION['ascending'], STATUS['all']),
        ([''], 100, ORDER_BY['by_creator'], ORDER_DIRECTION['descending'], STATUS['all']),
        ([''], 100, ORDER_BY['by_creator'], True, STATUS['all']),

        # STATUS
        ([''], 100, ORDER_BY['by_creator'], ORDER_DIRECTION['ascending'], STATUS['all']),
        ([''], 100, ORDER_BY['by_creator'], ORDER_DIRECTION['ascending'], STATUS['votable']),
        ([''], 100, ORDER_BY['by_creator'], ORDER_DIRECTION['ascending'], True),
]


@pytest.mark.parametrize(
<<<<<<< HEAD
    'start, limit, order_by, order_direction, status', CORRECT_VALUES
)
def tests_with_correct_values(node, wallet, start, limit, order_by, order_direction, status):
    proposals_tools.create_accounts_with_vests_and_tbd(wallet, ACCOUNTS)
    proposals_tools.prepare_proposals(wallet, ACCOUNTS)
=======
    'start, limit, order_by, order_direction, status', [
        *CORRECT_VALUES,
        *as_string(CORRECT_VALUES),
        ([True], 100, ORDER_BY['by_total_votes'], ORDER_DIRECTION['ascending'], STATUS['all']),
        ([''], True, ORDER_BY['by_creator'], ORDER_DIRECTION['ascending'], STATUS['all']),
        ([''], 100, ORDER_BY['by_creator'], True, STATUS['all']),
        ([''], 100, ORDER_BY['by_creator'], ORDER_DIRECTION['ascending'], True),
    ]
)
def test_list_proposals_with_correct_values(node, wallet, start, limit, order_by, order_direction, status):
    create_accounts_with_vests_and_tbd(wallet, ACCOUNTS)
    prepare_proposals(wallet, ACCOUNTS)
>>>>>>> 8bf85f216 (fixup! Rebuild test_list_proposal_votes.py)
    node.api.wallet_bridge.list_proposals(start, limit, order_by, order_direction, status)


@pytest.mark.parametrize(
    'start, limit, order_by, order_direction, status', CORRECT_VALUES
)
def tests_with_correct_values_with_quotes(node, wallet, start, limit, order_by, order_direction, status):
    proposals_tools.create_accounts_with_vests_and_tbd(wallet, ACCOUNTS)
    proposals_tools.prepare_proposals(wallet, ACCOUNTS)

    for start_number in range(len(start)):
        start[start_number] = proposals_tools.convert_bool_or_numeric_to_string(start[start_number])

    limit = proposals_tools.convert_bool_or_numeric_to_string(limit)
    order_by = proposals_tools.convert_bool_or_numeric_to_string(order_by)
    order_direction = proposals_tools.convert_bool_or_numeric_to_string(order_direction)
    status = proposals_tools.convert_bool_or_numeric_to_string(status)

    if limit == 'True' or order_direction == 'True' or status == 'True':   # Bool in quotes have special work and not throw exception
        with pytest.raises(test_tools.exceptions.CommunicationError):
            node.api.wallet_bridge.list_proposals(start, limit, order_by, order_direction, status)
    else:
        node.api.wallet_bridge.list_proposals(start, limit, order_by, order_direction, status)


@pytest.mark.parametrize(
    'start, limit, order_by, order_direction, status', [
        # LIMIT
        ([''], -1, ORDER_BY['by_creator'], ORDER_DIRECTION['ascending'], STATUS['all']),
        ([''], 1001, ORDER_BY['by_creator'], ORDER_DIRECTION['ascending'], STATUS['all']),

        # ORDER BY
        ([''], 100, 28, ORDER_DIRECTION['ascending'], STATUS['all']),
        ([''], 100, 32, ORDER_DIRECTION['ascending'], STATUS['all']),
        ([''], 100, True, ORDER_DIRECTION['ascending'], STATUS['all']),

        # ORDER DIRECTION
        ([''], 100, ORDER_BY['by_creator'], -1, STATUS['all']),
        ([''], 100, ORDER_BY['by_creator'], 2, STATUS['all']),

        # STATUS
        ([''], 100, ORDER_BY['by_creator'], ORDER_DIRECTION['ascending'], -1),
        ([''], 100, ORDER_BY['by_creator'], ORDER_DIRECTION['ascending'], 5),
    ],
)
<<<<<<< HEAD
def tests_with_incorrect_values(node, wallet, start, limit, order_by, order_direction, status):
    proposals_tools.create_accounts_with_vests_and_tbd(wallet, ACCOUNTS)
    proposals_tools.prepare_proposals(wallet, ACCOUNTS)
=======
def test_list_proposals_with_incorrect_values(node, wallet, start, limit, order_by, order_direction, status):
    create_accounts_with_vests_and_tbd(wallet, ACCOUNTS)
    prepare_proposals(wallet, ACCOUNTS)
>>>>>>> 8bf85f216 (fixup! Rebuild test_list_proposal_votes.py)
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

        # ORDER TYPE
        ([""], 100, 'invalid-argument', 0, 0),

        # ORDER DIRECTION
        ([""], 100, 29, 'invalid-argument', 0),

        # STATUS
        ([""], 100, 29, 0, 'invalid-argument'),
    ]
)
def tests_with_incorrect_type_of_argument(node, start, limit, order_by, order_direction, status):
    with pytest.raises(test_tools.exceptions.CommunicationError):
        node.api.wallet_bridge.list_proposals(start, limit, order_by, order_direction, status)
