from datetime import datetime, timedelta

import pytest

import test_tools.exceptions
from test_tools import Asset

# TODO BUG LIST!
"""
1. Problem with running wallet_bridge_api.list_proposal_votes with empty string in start in sorting by id. 
   ("['']", 100, 'by_proposal_voter', 'ascending', 'all') (# BUG1)
   
2. Problem with running wallet_bridge_api.list_proposal_votes with negative value in start in sorting by id. 
   ("[-2]", 100, 'by_proposal_voter', 'ascending', 'all') (# BUG2)
   
3. Problem with running wallet_bridge_api.find_proposals with incorrect number of id. (# BUG3)

4. Problem with calling wallet_bridge_api.list_proposals with the wrong data types in the arguments(# BUG4)

5. Problem with calling wallet_bridge_api.list_proposal_votes with the wrong data types in the arguments(# BUG5)

6. Problem with calling wallet_bridge_api.find_proposals with the wrong data types in the arguments(# BUG6)
"""
ACCOUNTS = [f'account-{i}' for i in range(5)]

ORDER_BY_LIST_PROPOSALS = {
    'by_creator': 29,
    'by_start_date': 30,
    'by_end_date': 31
}

ORDER_BY_LIST_PROPOSAL_VOTES = {
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


def date_from_now(*, weeks):
    future_data = datetime.now() + timedelta(weeks=weeks)
    return future_data.strftime('%Y-%m-%dT%H:%M:%S')


@pytest.mark.parametrize(
    'start, limit, order_by_list_proposals, order_direction, status', [
        # START
        # by creator
        ([''], 100, ORDER_BY_LIST_PROPOSALS['by_creator'], ORDER_DIRECTION['ascending'], STATUS['all']),
        ([ACCOUNTS[1]], 100, ORDER_BY_LIST_PROPOSALS['by_creator'], ORDER_DIRECTION['ascending'], STATUS['all']),
        # Start from nonexistent account (name "account-2a" is alphabetically between 'account-2' and 'account-3').
        (['account-2a'], 100, ORDER_BY_LIST_PROPOSALS['by_creator'], ORDER_DIRECTION['ascending'], STATUS['all']),
        ([ACCOUNTS[1], date_from_now(weeks=20)], 100, ORDER_BY_LIST_PROPOSALS['by_creator'],
         ORDER_DIRECTION['ascending'], STATUS['all']),
        ([ACCOUNTS[1], date_from_now(weeks=25)], 100, ORDER_BY_LIST_PROPOSALS['by_creator'],
         ORDER_DIRECTION['ascending'], STATUS['all']),
        ([ACCOUNTS[1], date_from_now(weeks=20), date_from_now(weeks=25)], 100, ORDER_BY_LIST_PROPOSALS['by_creator'],
         ORDER_DIRECTION['ascending'], STATUS['all']),
        ([ACCOUNTS[1], date_from_now(weeks=20), date_from_now(weeks=25), 'additional_argument'], 100,
         ORDER_BY_LIST_PROPOSALS['by_creator'], ORDER_DIRECTION['ascending'], STATUS['all']),

        # by start date
        ([], 100, 'by_start_date', 'ascending', 'all'),
        ([date_from_now(weeks=20)], 100, ORDER_BY_LIST_PROPOSALS['by_start_date'], ORDER_DIRECTION['ascending'],
         STATUS['all']),
        ([date_from_now(weeks=20), date_from_now(weeks=25)], 100, ORDER_BY_LIST_PROPOSALS['by_start_date'],
         ORDER_DIRECTION['ascending'], STATUS['all']),
        ([date_from_now(weeks=20), ACCOUNTS[1]], 100, ORDER_BY_LIST_PROPOSALS['by_start_date'],
         ORDER_DIRECTION['ascending'], STATUS['all']),
        ([date_from_now(weeks=20), ACCOUNTS[1], date_from_now(weeks=25)], 100, ORDER_BY_LIST_PROPOSALS['by_start_date'],
         ORDER_DIRECTION['ascending'], STATUS['all']),
        ([date_from_now(weeks=20), ACCOUNTS[1], date_from_now(weeks=25), 'additional_argument'], 100,
         ORDER_BY_LIST_PROPOSALS['by_start_date'], ORDER_DIRECTION['ascending'], STATUS['all']),

        # by end date
        ([], 100, ORDER_BY_LIST_PROPOSALS['by_end_date'], ORDER_DIRECTION['ascending'], STATUS['all']),
        ([date_from_now(weeks=25)], 100, ORDER_BY_LIST_PROPOSALS['by_end_date'], ORDER_DIRECTION['ascending'],
         STATUS['all']),
        ([date_from_now(weeks=25), date_from_now(weeks=20)], 100, ORDER_BY_LIST_PROPOSALS['by_end_date'],
         ORDER_DIRECTION['ascending'], STATUS['all']),
        ([date_from_now(weeks=25), ACCOUNTS[1]], 100, ORDER_BY_LIST_PROPOSALS['by_end_date'],
         ORDER_DIRECTION['ascending'], STATUS['all']),
        ([date_from_now(weeks=25), ACCOUNTS[1], date_from_now(weeks=25)], 100, ORDER_BY_LIST_PROPOSALS['by_end_date'],
         ORDER_DIRECTION['ascending'], STATUS['all']),
        ([date_from_now(weeks=25), ACCOUNTS[1], date_from_now(weeks=25), 'additional_argument'], 100,
         ORDER_BY_LIST_PROPOSALS['by_end_date'], ORDER_DIRECTION['ascending'], STATUS['all']),

        # LIMIT
        ([''], 0, ORDER_BY_LIST_PROPOSALS['by_creator'], ORDER_DIRECTION['ascending'], STATUS['all']),
        ([''], 1000, ORDER_BY_LIST_PROPOSALS['by_creator'], ORDER_DIRECTION['ascending'], STATUS['all']),

        # ORDER BY
        ([''], 100, ORDER_BY_LIST_PROPOSALS['by_creator'], ORDER_DIRECTION['ascending'], STATUS['all']),
        ([''], 100, ORDER_BY_LIST_PROPOSALS['by_end_date'], ORDER_DIRECTION['ascending'], STATUS['all']),

        # ORDER DIRECTION
        ([''], 100, ORDER_BY_LIST_PROPOSALS['by_creator'], ORDER_DIRECTION['ascending'], STATUS['all']),
        ([''], 100, ORDER_BY_LIST_PROPOSALS['by_creator'], ORDER_DIRECTION['descending'], STATUS['all']),

        # STATUS
        ([''], 100, ORDER_BY_LIST_PROPOSALS['by_creator'], ORDER_DIRECTION['ascending'], STATUS['all']),
        ([''], 100, ORDER_BY_LIST_PROPOSALS['by_creator'], ORDER_DIRECTION['ascending'], STATUS['votable']),
    ]
)
def test_list_proposals_with_correct_values(node, wallet, start, limit, order_by_list_proposals,
                                            order_direction, status):
    create_accounts_with_vests_and_tbd(wallet, len(ACCOUNTS))
    prepare_proposals(wallet, ACCOUNTS)
    node.api.wallet_bridge.list_proposals(start, limit, order_by_list_proposals, order_direction, status)


@pytest.mark.parametrize(
    'start, limit, order_by_list_proposals, order_direction, status', [
        # LIMIT
        ([''], -1, ORDER_BY_LIST_PROPOSALS['by_creator'], ORDER_DIRECTION['ascending'], STATUS['all']),
        ([''], 1001, ORDER_BY_LIST_PROPOSALS['by_creator'], ORDER_DIRECTION['ascending'], STATUS['all']),

        # ORDER BY
        ([''], 100, 28, ORDER_DIRECTION['ascending'], STATUS['all']),
        ([''], 100, 32, ORDER_DIRECTION['ascending'], STATUS['all']),

        # ORDER DIRECTION
        ([''], 100, ORDER_BY_LIST_PROPOSALS['by_creator'], -1, STATUS['all']),
        ([''], 100, ORDER_BY_LIST_PROPOSALS['by_creator'], 2, STATUS['all']),

        # STATUS
        ([''], 100, ORDER_BY_LIST_PROPOSALS['by_creator'], ORDER_DIRECTION['ascending'], -1),
        ([''], 100, ORDER_BY_LIST_PROPOSALS['by_creator'], ORDER_DIRECTION['ascending'], 5),
    ],
)
def test_list_proposals_with_incorrect_values(node, wallet, start, limit, order_by_list_proposals,
                                              order_direction, status):
    create_accounts_with_vests_and_tbd(wallet, len(ACCOUNTS))
    prepare_proposals(wallet, ACCOUNTS)
    with pytest.raises(test_tools.exceptions.CommunicationError):
        node.api.wallet_bridge.list_proposals(start, limit, order_by_list_proposals, order_direction, status)


@pytest.mark.parametrize(
    'start, limit, order_by_list_proposal_votes, order_direction, status', [
        # START
        # by_voter_proposal
        ([''], 100, ORDER_BY_LIST_PROPOSAL_VOTES['by_voter_proposal'], ORDER_DIRECTION['ascending'], STATUS['all']),
        ([ACCOUNTS[1]], 100, ORDER_BY_LIST_PROPOSAL_VOTES['by_voter_proposal'], ORDER_DIRECTION['ascending'],
         STATUS['all']),
        # # Start from nonexistent account (name "account-2a" is alphabetically between 'account-2' and 'account-3').
        (['account-2a'], 100, ORDER_BY_LIST_PROPOSAL_VOTES['by_voter_proposal'], ORDER_DIRECTION['ascending'],
         STATUS['all']),
        ([ACCOUNTS[1], 3], 100, ORDER_BY_LIST_PROPOSAL_VOTES['by_voter_proposal'], ORDER_DIRECTION['ascending'],
         STATUS['all']),
        ([ACCOUNTS[1], 3, 'additional_argument'], 100, ORDER_BY_LIST_PROPOSAL_VOTES['by_voter_proposal'],
         ORDER_DIRECTION['ascending'], STATUS['all']),

        # by proposal voter
        ([''], 100, ORDER_BY_LIST_PROPOSAL_VOTES['by_proposal_voter'], ORDER_DIRECTION['ascending'], STATUS['all']),
        ([3], 100, ORDER_BY_LIST_PROPOSAL_VOTES['by_proposal_voter'], ORDER_DIRECTION['ascending'], STATUS['all']),
        ([3, ACCOUNTS[1]], 100, ORDER_BY_LIST_PROPOSAL_VOTES['by_proposal_voter'], ORDER_DIRECTION['ascending'],
         STATUS['all']),
        ([3, ACCOUNTS[1], 'additional_argument'], 100, ORDER_BY_LIST_PROPOSAL_VOTES['by_proposal_voter'],
         ORDER_DIRECTION['ascending'], STATUS['all']),

        # LIMIT
        ([''], 0, ORDER_BY_LIST_PROPOSAL_VOTES['by_voter_proposal'], ORDER_DIRECTION['ascending'], STATUS['all']),
        ([''], 1000, ORDER_BY_LIST_PROPOSAL_VOTES['by_voter_proposal'], ORDER_DIRECTION['ascending'], STATUS['all']),

        # ORDER BY
        ([''], 100, ORDER_BY_LIST_PROPOSAL_VOTES['by_voter_proposal'], ORDER_DIRECTION['ascending'], STATUS['all']),
        # BUG with order type ('by_proposal_voter')
        # ([''], 100, ORDER_BY_LIST_PROPOSAL_VOTES['by_proposal_voter'], ORDER_DIRECTION['ascending'], STATUS['all']),   # BUG1

        # ORDER DIRECTION
        ([''], 100, ORDER_BY_LIST_PROPOSAL_VOTES['by_proposal_voter'], ORDER_DIRECTION['ascending'], STATUS['all']),
        ([''], 100, ORDER_BY_LIST_PROPOSAL_VOTES['by_proposal_voter'], ORDER_DIRECTION['descending'], STATUS['all']),

        # STATUS
        ([''], 100, ORDER_BY_LIST_PROPOSAL_VOTES['by_proposal_voter'], ORDER_DIRECTION['ascending'], STATUS['all']),
        ([''], 100, ORDER_BY_LIST_PROPOSAL_VOTES['by_proposal_voter'], ORDER_DIRECTION['ascending'], STATUS['votable']),
    ],
)
def test_list_proposal_votes_with_correct_values(node, wallet, start, limit, order_by_list_proposal_votes,
                                                 order_direction, status):
    create_accounts_with_vests_and_tbd(wallet, len(ACCOUNTS))
    prepare_proposals(wallet, ACCOUNTS)
    with wallet.in_single_transaction():
        for account in ACCOUNTS:
            wallet.api.update_proposal_votes(account, [3], 1)

    node.api.wallet_bridge.list_proposal_votes(start, limit, order_by_list_proposal_votes, order_direction, status)


@pytest.mark.parametrize(
    'start, limit, order_by_list_proposal_votes, order_direction, status', [
        # START
        # ([-2], 100, ORDER_BY_LIST_PROPOSAL_VOTES['by_proposal_voter'], ORDER_DIRECTION['ascending'], STATUS['all']),  # BUG2

        # LIMIT
        ([''], -1, ORDER_BY_LIST_PROPOSAL_VOTES['by_voter_proposal'], ORDER_DIRECTION['ascending'], STATUS['all']),
        ([''], 1001, ORDER_BY_LIST_PROPOSAL_VOTES['by_voter_proposal'], ORDER_DIRECTION['ascending'], STATUS['all']),

        # ORDER BY
        ([''], 100, 32, ORDER_DIRECTION['ascending'], STATUS['all']),
        ([''], 100, 35, ORDER_DIRECTION['ascending'], STATUS['all']),

        # ORDER DIRECTION
        ([''], 100, ORDER_BY_LIST_PROPOSAL_VOTES['by_voter_proposal'], -1, STATUS['all']),
        ([''], 100, ORDER_BY_LIST_PROPOSAL_VOTES['by_voter_proposal'], 2, STATUS['all']),

        # STATUS
        ([''], 100, ORDER_BY_LIST_PROPOSAL_VOTES['by_voter_proposal'], ORDER_DIRECTION['ascending'], -1),
        ([''], 100, ORDER_BY_LIST_PROPOSAL_VOTES['by_voter_proposal'], ORDER_DIRECTION['ascending'], 5),
    ],
)
def test_list_proposal_votes_with_incorrect_values(node, wallet, start, limit, order_by_list_proposal_votes,
                                                   order_direction, status):
    create_accounts_with_vests_and_tbd(wallet, len(ACCOUNTS))
    prepare_proposals(wallet, ACCOUNTS)
    with wallet.in_single_transaction():
        for account in ACCOUNTS:
            wallet.api.update_proposal_votes(account, [3], 1)

    with pytest.raises(test_tools.exceptions.CommunicationError):
        node.api.wallet_bridge.list_proposal_votes(start, limit, order_by_list_proposal_votes, order_direction, status)

@pytest.mark.parametrize(
    'proposal_id', [
        0,
        (0, 1, 2, 3, 4),
        6,
    ]
)
def test_find_proposals_with_correct_values(node, wallet, proposal_id):
    create_accounts_with_vests_and_tbd(wallet, len(ACCOUNTS))
    prepare_proposals(wallet, ACCOUNTS)

    node.api.wallet_bridge.find_proposals([proposal_id])


@pytest.mark.parametrize(
    'proposal_id', [
        -1,  # OUT OFF LIMITS: too low id
    ]
)
def test_find_proposals_with_incorrect_values(node, wallet, proposal_id):
    create_accounts_with_vests_and_tbd(wallet, len(ACCOUNTS))
    prepare_proposals(wallet, ACCOUNTS)

    with pytest.raises(test_tools.exceptions.CommunicationError):
        node.api.wallet_bridge.find_proposals([proposal_id])


@pytest.mark.parametrize(
    'start, limit, order_by, order_direction, status', [
        # START
        (10, 100, 29, 0, 0),
        ('invalid-argument', 100, 29, 0, 0),
        (True, 100, 29, 0, 0),

        # LIMIT
        ([""], 'invalid-argument', 29, 0, 0),
        # ([""], True, 29, 0, 0),  # BUG4

        # ORDER TYPE
        ([""], 100, 'invalid-argument', 0, 0),
        ([""], 100, True, 0, 0),

        # ORDER DIRECTION
        ([""], 100, 29, 'invalid-argument', 0),
        # ([""], 100, 29, True, 0),  # BUG4

        # STATUS
        ([""], 100, 29, 0, 'invalid-argument'),
        # ([""], 100, 29, 0, True),  # BUG4
    ]
)
def test_list_proposals_with_incorrect_type_of_argument(node, start, limit, order_by, order_direction, status):
    with pytest.raises(test_tools.exceptions.CommunicationError):
        node.api.wallet_bridge.list_proposals(start, limit, order_by, order_direction, status)


@pytest.mark.parametrize(
    'start, limit, order_by, order_direction, status', [
        # START
        (10, 100, 33, 0, 0),
        ('invalid-argument', 100, 33, 0, 0),
        (True, 100, 33, 0, 0),

        # LIMIT
        ([""], 'invalid-argument', 33, 0, 0),
        # ([""], True, 33, 0, 0),  # BUG5

        # ORDER TYPE
        ([""], 100, 'invalid-argument', 0, 0),
        ([""], 100, True, 0, 0),

        # ORDER DIRECTION
        ([""], 100, 33, 'invalid-argument', 0),
        # ([""], 100, 33, True, 0),  # BUG5

        # STATUS
        ([""], 100, 33, 0, 'invalid-argument'),
        # ([""], 100, 33, 0, True),  # BUG5
    ]
)
def test_list_proposal_votes_with_incorrect_type_of_argument(node, start, limit, order_by, order_direction, status):
    with pytest.raises(test_tools.exceptions.CommunicationError):
        node.api.wallet_bridge.list_proposal_votes(start, limit, order_by, order_direction, status)


@pytest.mark.parametrize(
    'proposal_id', [
        'invalid-argument',
        # True,  # BUG6
    ]
)
def test_find_proposals_with_incorrect_type_of_argument(node, proposal_id):
    with pytest.raises(test_tools.exceptions.CommunicationError):
        node.api.wallet_bridge.find_proposals([proposal_id])


def create_accounts_with_vests_and_tbd(wallet, number_of_accounts):
    wallet.create_accounts(number_of_accounts)
    with wallet.in_single_transaction():
        for account in ACCOUNTS:
            wallet.api.transfer_to_vesting('initminer', account, Asset.Test(10000))

    with wallet.in_single_transaction():
        for account in ACCOUNTS:
            wallet.api.transfer('initminer', account, Asset.Tbd(10000), 'memo')


def prepare_proposals(wallet, accounts):
    with wallet.in_single_transaction():
        for account in accounts:
            wallet.api.post_comment(account, 'permlink', '', 'parent-permlink', 'title', 'body', '{}')

    with wallet.in_single_transaction():
        for account_number in range(len(accounts)):
            wallet.api.create_proposal(accounts[account_number],
                                       accounts[account_number],
                                       date_from_now(weeks=account_number * 10),
                                       date_from_now(weeks=account_number * 10 + 5),
                                       Asset.Tbd(account_number * 100),
                                       f'subject-{account_number}',
                                       'permlink')


def get_accounts_name(accounts):
    return [account.name for account in accounts]
