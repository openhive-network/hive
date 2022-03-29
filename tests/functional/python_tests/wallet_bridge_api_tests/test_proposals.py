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

@pytest.fixture
def order_by_list_proposals(request):
    return {
        'by_creator': 29,
        'by_start_date': 30,
        'by_end_date': 31
    }.get(request.param, request.param)


@pytest.fixture
def order_by_list_proposal_votes(request):
    return {
        'by_voter_proposal': 33,
        'by_proposal_voter': 34,
    }.get(request.param, request.param)


@pytest.fixture
def order_direction(request):
    return {
        'ascending': 0,
        'descending': 1
    }.get(request.param, request.param)


@pytest.fixture
def status(request):
    return {
        'all': 0,
        'inactive': 1,
        'active': 2,
        'expired': 3,
        'votable': 4
    }.get(request.param, request.param)


@pytest.fixture
def accounts(wallet):
    return create_accounts_with_vests_and_tbd(wallet, 5)


@pytest.fixture
def start(accounts, request):
    return eval(request.param)


@pytest.mark.parametrize(
    'start, limit, order_by_list_proposals, order_direction, status', [
        # START
            # by creator
        ("[]", 100, 'by_creator', 'ascending', 'all'),
        ("[accounts[1]]", 100, 'by_creator', 'ascending', 'all'),
        # Start from nonexistent account (name "account-2a" is alphabetically between 'account-2' and 'account-3').
        ("['account-2a']", 100, 'by_creator', 'ascending', 'all'),
        ("[accounts[1], date_from_now(weeks=20)]", 100, 'by_creator', 'ascending', 'all'),
        ("[accounts[1], date_from_now(weeks=25)]", 100, 'by_creator', 'ascending', 'all'),
        ("[accounts[1], date_from_now(weeks=20), date_from_now(weeks=25)]", 100, 'by_creator', 'ascending', 'all'),
        ("[accounts[1], date_from_now(weeks=20), date_from_now(weeks=25), 'additional_argument']", 100, 'by_creator', 'ascending', 'all'),

        # by start date
        ("[]", 100, 'by_start_date', 'ascending', 'all'),
        ("[date_from_now(weeks=20)]", 100, 'by_start_date', 'ascending', 'all'),
        ("[date_from_now(weeks=20), date_from_now(weeks=25)]", 100, 'by_start_date', 'ascending', 'all'),
        ("[date_from_now(weeks=20), accounts[1]]", 100, 'by_start_date', 'ascending', 'all'),
        ("[date_from_now(weeks=20), accounts[1], date_from_now(weeks=25)]", 100, 'by_start_date', 'ascending', 'all'),
        ("[date_from_now(weeks=20), accounts[1], date_from_now(weeks=25), 'additional_argument']", 100, 'by_start_date', 'ascending', 'all'),

        # by end date
        ("[]", 100, 'by_end_date', 'ascending', 'all'),
        ("[date_from_now(weeks=25)]", 100, 'by_end_date', 'ascending', 'all'),
        ("[date_from_now(weeks=25), date_from_now(weeks=20)]", 100, 'by_end_date', 'ascending', 'all'),
        ("[date_from_now(weeks=25), accounts[1]]", 100, 'by_end_date', 'ascending', 'all'),
        ("[date_from_now(weeks=25), accounts[1], date_from_now(weeks=25)]", 100, 'by_end_date', 'ascending', 'all'),
        ("[date_from_now(weeks=25), accounts[1], date_from_now(weeks=25), 'additional_argument']", 100, 'by_end_date', 'ascending', 'all'),

        # LIMIT
        ("['']", 0, 'by_creator', 'ascending', 'all'),
        ("['']", 1000, 'by_creator', 'ascending', 'all'),

        # ORDER BY
        ("['']", 100, 'by_creator', 'ascending', 'all'),
        ("['']", 100, 'by_end_date', 'ascending', 'all'),

        # ORDER DIRECTION
        ("['']", 100, 'by_creator', 'ascending', 'all'),
        ("['']", 100, 'by_creator', 'descending', 'all'),

        # STATUS
        ("['']", 100, 'by_creator', 'ascending', 'all'),
        ("['']", 100, 'by_creator', 'ascending', 'votable'),
    ],
    indirect=['start', 'order_by_list_proposals', 'order_direction', 'status']
)
def test_list_proposals_with_correct_values(node, accounts, wallet, start, limit, order_by_list_proposals,
                                            order_direction, status):
    prepare_proposals(wallet, accounts)
    node.api.wallet_bridge.list_proposals(start, limit, order_by_list_proposals, order_direction, status)


@pytest.mark.parametrize(
    'start, limit, order_by_list_proposals, order_direction, status', [
        # LIMIT
        ("['']", -1, 'by_creator', 'ascending', 'all'),
        ("['']", 1001, 'by_creator', 'ascending', 'all'),

        # ORDER BY
        ("['']", 100, 28, 'ascending', 'all'),
        ("['']", 100, 32, 'ascending', 'all'),

        # ORDER DIRECTION
        ("['']", 100, 'by_creator', -1, 'all'),
        ("['']", 100, 'by_creator', 2, 'all'),

        # STATUS
        ("['']", 100, 'by_creator', 'ascending', -1),
        ("['']", 100, 'by_creator', 'ascending', 5),
    ],
    indirect=['start', 'order_by_list_proposals', 'order_direction', 'status']
)
def test_list_proposals_with_incorrect_values(node, accounts, wallet, start, limit, order_by_list_proposals,
                                              order_direction, status):
    prepare_proposals(wallet, accounts)
    with pytest.raises(test_tools.exceptions.CommunicationError):
        node.api.wallet_bridge.list_proposals(start, limit, order_by_list_proposals, order_direction, status)


@pytest.mark.parametrize(
    'start, limit, order_by_list_proposal_votes, order_direction, status', [
        # START
        ("[accounts[1]]", 100, 'by_voter_proposal', 'ascending', 'all'),
        # Start from nonexistent account (name "account-2a" is alphabetically between 'account-2' and 'account-3').
        ("['account-2a']", 100, 'by_voter_proposal', 'ascending', 'all'),
        ("[3]", 100, 'by_proposal_voter', 'ascending', 'all'),

        # LIMIT
        ("['']", 0, 'by_voter_proposal', 'ascending', 'all'),
        ("['']", 1000, 'by_voter_proposal', 'ascending', 'all'),

        # ORDER BY
        ("['']", 100, 'by_voter_proposal', 'ascending', 'all'),
        # BUG with order type ('by_proposal_voter')
        # ("['']", 100, 'by_proposal_voter', 'ascending', 'all'), # BUG1

        # ORDER DIRECTION
        ("['']", 100, 'by_voter_proposal', 'ascending', 'all'),
        ("['']", 100, 'by_voter_proposal', 'descending', 'all'),

        # STATUS
        ("['']", 100, 'by_voter_proposal', 'ascending', 'all'),
        ("['']", 100, 'by_voter_proposal', 'ascending', 'votable'),
    ],
    indirect=['start', 'order_by_list_proposal_votes', 'order_direction', 'status']
)
def test_list_proposal_votes_with_correct_values(node, accounts, wallet, start, limit, order_by_list_proposal_votes,
                                                 order_direction, status):
    prepare_proposals(wallet, accounts)
    with wallet.in_single_transaction():
        for account in accounts:
            wallet.api.update_proposal_votes(account, [3], 1)

    node.api.wallet_bridge.list_proposal_votes(start, limit, order_by_list_proposal_votes, order_direction, status)


@pytest.mark.parametrize(
    'start, limit, order_by_list_proposal_votes, order_direction, status', [
        # START
        # ("[-2]", 100, 'by_proposal_voter', 'ascending', 'all'), # BUG2

        # LIMIT
        ("['']", -1, 'by_voter_proposal', 'ascending', 'all'),
        ("['']", 1001, 'by_voter_proposal', 'ascending', 'all'),

        # ORDER BY
        ("['']", 100, 32, 'ascending', 'all'),
        ("['']", 100, 35, 'ascending', 'all'),

        # ORDER DIRECTION
        ("['']", 100, 'by_voter_proposal', -1, 'all'),
        ("['']", 100, 'by_voter_proposal', 2, 'all'),

        # STATUS
        ("['']", 100, 'by_voter_proposal', 'ascending', -1),
        ("['']", 100, 'by_voter_proposal', 'ascending', 5),
    ],
    indirect=['start', 'order_by_list_proposal_votes', 'order_direction', 'status']
)
def test_list_proposal_votes_with_incorrect_values(node, accounts, wallet, start, limit, order_by_list_proposal_votes,
                                                   order_direction,
                                                   status):
    prepare_proposals(wallet, accounts)
    with wallet.in_single_transaction():
        for account in accounts:
            wallet.api.update_proposal_votes(account, [3], 1)

    with pytest.raises(test_tools.exceptions.CommunicationError):
        node.api.wallet_bridge.list_proposal_votes(start, limit, order_by_list_proposal_votes, order_direction, status)


def test_find_proposals_with_correct_values(node, wallet):
    accounts = create_accounts_with_vests_and_tbd(wallet, 5)
    prepare_proposals(wallet, accounts)

    node.api.wallet_bridge.find_proposals([0, 1, 2, 3, 4])


@pytest.mark.parametrize(
    'proposal_id', [
        # -1,  # OUT OFF LIMITS: too low id (# BUG3)
        # 6,  # OUT OFF LIMITS: too big id (proposal with this id does not exist) (# BUG3)
    ]
)
def test_find_proposals_with_incorrect_values(node, wallet, proposal_id):
    accounts = create_accounts_with_vests_and_tbd(wallet, 5)
    prepare_proposals(wallet, accounts)

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

        #ORDER DIRECTION
        ([""], 100, 29, 'invalid-argument', 0),
        # ([""], 100, 29, True, 0),  # BUG4

        #STATUS
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
    accounts = get_accounts_name(wallet.create_accounts(number_of_accounts, 'account'))
    with wallet.in_single_transaction():
        for account in accounts:
            wallet.api.transfer_to_vesting('initminer', account, Asset.Test(10000))

    with wallet.in_single_transaction():
        for account in accounts:
            wallet.api.transfer('initminer', account, Asset.Tbd(10000), 'memo')

    return accounts


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


def date_from_now(*, weeks):
    future_data = datetime.now() + timedelta(weeks=weeks)
    return future_data.strftime('%Y-%m-%dT%H:%M:%S')
