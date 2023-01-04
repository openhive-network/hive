import pytest

import test_tools as tt

from hive_local_tools.api.message_format import as_string
from hive_local_tools.api.message_format.wallet_bridge_api import create_accounts_with_vests_and_tbd, prepare_proposals

ACCOUNTS = [f"account-{i}" for i in range(5)]

ORDER_BY = {
    "by_voter_proposal": 33,
    "by_proposal_voter": 34,
}

ORDER_DIRECTION = {"ascending": 0, "descending": 1}

STATUS = {"all": 0, "inactive": 1, "active": 2, "expired": 3, "votable": 4}

CORRECT_VALUES = [
    # START
    # by_voter_proposal
    ([""], 100, ORDER_BY["by_voter_proposal"], ORDER_DIRECTION["ascending"], STATUS["all"]),
    (["true"], 100, ORDER_BY["by_voter_proposal"], ORDER_DIRECTION["ascending"], STATUS["all"]),
    ([10], 100, ORDER_BY["by_voter_proposal"], ORDER_DIRECTION["ascending"], STATUS["all"]),
    (["invalid-account-name"], 100, ORDER_BY["by_voter_proposal"], ORDER_DIRECTION["ascending"], STATUS["all"]),
    ([ACCOUNTS[1]], 100, ORDER_BY["by_voter_proposal"], ORDER_DIRECTION["ascending"], STATUS["all"]),
    # Start from nonexistent account (name "account-2a" is alphabetically between 'account-2' and 'account-3').
    (["account-2a"], 100, ORDER_BY["by_voter_proposal"], ORDER_DIRECTION["ascending"], STATUS["all"]),
    ([ACCOUNTS[1], 4], 100, ORDER_BY["by_voter_proposal"], ORDER_DIRECTION["ascending"], STATUS["all"]),
    (
        [ACCOUNTS[1], 3, "additional_argument"],
        100,
        ORDER_BY["by_voter_proposal"],
        ORDER_DIRECTION["ascending"],
        STATUS["all"],
    ),
    # by proposal voter
    ([3], 100, ORDER_BY["by_proposal_voter"], ORDER_DIRECTION["ascending"], STATUS["all"]),
    ([3, ACCOUNTS[1]], 100, ORDER_BY["by_proposal_voter"], ORDER_DIRECTION["ascending"], STATUS["all"]),
    (
        [3, ACCOUNTS[1], "additional_argument"],
        100,
        ORDER_BY["by_proposal_voter"],
        ORDER_DIRECTION["ascending"],
        STATUS["all"],
    ),
    # LIMIT
    ([""], 1, ORDER_BY["by_voter_proposal"], ORDER_DIRECTION["ascending"], STATUS["all"]),
    ([""], 1000, ORDER_BY["by_voter_proposal"], ORDER_DIRECTION["ascending"], STATUS["all"]),
    # ORDER BY
    ([""], 100, ORDER_BY["by_voter_proposal"], ORDER_DIRECTION["ascending"], STATUS["all"]),
    ([3], 100, ORDER_BY["by_proposal_voter"], ORDER_DIRECTION["ascending"], STATUS["all"]),
    # ORDER DIRECTION
    ([""], 100, ORDER_BY["by_voter_proposal"], ORDER_DIRECTION["ascending"], STATUS["all"]),
    ([""], 100, ORDER_BY["by_voter_proposal"], ORDER_DIRECTION["descending"], STATUS["all"]),
    # STATUS
    ([""], 100, ORDER_BY["by_voter_proposal"], ORDER_DIRECTION["ascending"], STATUS["all"]),
    ([""], 100, ORDER_BY["by_voter_proposal"], ORDER_DIRECTION["ascending"], STATUS["votable"]),
]


@pytest.mark.parametrize(
    "start, limit, order_by, order_direction, status",
    [
        *CORRECT_VALUES,
        *as_string(CORRECT_VALUES),
        ([True], 100, ORDER_BY["by_proposal_voter"], ORDER_DIRECTION["ascending"], STATUS["all"]),
        ([True], 100, ORDER_BY["by_voter_proposal"], ORDER_DIRECTION["ascending"], STATUS["all"]),
        ([""], True, ORDER_BY["by_voter_proposal"], ORDER_DIRECTION["ascending"], STATUS["all"]),
        ([""], 100, ORDER_BY["by_voter_proposal"], True, STATUS["all"]),
        ([""], 100, ORDER_BY["by_voter_proposal"], ORDER_DIRECTION["ascending"], True),
    ],
)
def test_list_proposal_votes_with_correct_values(node, wallet, start, limit, order_by, order_direction, status):
    create_accounts_with_vests_and_tbd(wallet, ACCOUNTS)
    prepare_proposals(wallet, ACCOUNTS)
    with wallet.in_single_transaction():
        for account in ACCOUNTS:
            wallet.api.update_proposal_votes(account, [3], 1)

    node.api.wallet_bridge.list_proposal_votes(start, limit, order_by, order_direction, status)


@pytest.mark.parametrize(
    "start, limit, order_by, order_direction, status",
    [
        # START
        ([-2], 100, ORDER_BY["by_proposal_voter"], ORDER_DIRECTION["ascending"], STATUS["all"]),
        # LIMIT
        ([""], 0, ORDER_BY["by_voter_proposal"], ORDER_DIRECTION["ascending"], STATUS["all"]),
        ([""], 1001, ORDER_BY["by_voter_proposal"], ORDER_DIRECTION["ascending"], STATUS["all"]),
        ([""], "true", ORDER_BY["by_voter_proposal"], ORDER_DIRECTION["ascending"], STATUS["all"]),
        # ORDER BY
        ([""], 100, 32, ORDER_DIRECTION["ascending"], STATUS["all"]),
        ([""], 100, 35, ORDER_DIRECTION["ascending"], STATUS["all"]),
        ([""], 100, True, ORDER_DIRECTION["ascending"], STATUS["all"]),
        ([""], 100, "true", ORDER_DIRECTION["ascending"], STATUS["all"]),
        # ORDER DIRECTION
        ([""], 100, ORDER_BY["by_voter_proposal"], -1, STATUS["all"]),
        ([""], 100, ORDER_BY["by_voter_proposal"], 2, STATUS["all"]),
        ([""], 100, ORDER_BY["by_voter_proposal"], "true", STATUS["all"]),
        # STATUS
        ([""], 100, ORDER_BY["by_voter_proposal"], ORDER_DIRECTION["ascending"], -1),
        ([""], 100, ORDER_BY["by_voter_proposal"], ORDER_DIRECTION["ascending"], 5),
        ([""], 100, ORDER_BY["by_voter_proposal"], ORDER_DIRECTION["ascending"], "true"),
    ],
)
def test_list_proposal_votes_with_incorrect_values(node, wallet, start, limit, order_by, order_direction, status):
    create_accounts_with_vests_and_tbd(wallet, ACCOUNTS)
    prepare_proposals(wallet, ACCOUNTS)
    with wallet.in_single_transaction():
        for account in ACCOUNTS:
            wallet.api.update_proposal_votes(account, [3], 1)

    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.wallet_bridge.list_proposal_votes(start, limit, order_by, order_direction, status)


@pytest.mark.parametrize(
    "start, limit, order_by, order_direction, status",
    [
        # START
        # by_voter_proposal
        ("non-exist-acc", 100, ORDER_BY["by_voter_proposal"], 0, 0),
        (True, 100, ORDER_BY["by_voter_proposal"], 0, 0),
        (100, 100, ORDER_BY["by_voter_proposal"], 0, 0),
        ([{}], 100, ORDER_BY["by_voter_proposal"], 0, 0),
        ([[]], 100, ORDER_BY["by_voter_proposal"], 0, 0),
        # by proposal voter
        ("non-exist-acc", 100, ORDER_BY["by_proposal_voter"], 0, 0),
        (True, 100, ORDER_BY["by_proposal_voter"], 0, 0),
        (100, 100, ORDER_BY["by_proposal_voter"], 0, 0),
        ([{}], 100, ORDER_BY["by_proposal_voter"], 0, 0),
        ([[]], 100, ORDER_BY["by_proposal_voter"], 0, 0),
        ([""], 100, ORDER_BY["by_proposal_voter"], 0, 0),
        (["true"], 100, ORDER_BY["by_proposal_voter"], 0, 0),
        (["account2a"], 100, ORDER_BY["by_proposal_voter"], 0, 0),
        ([ACCOUNTS[1]], 100, ORDER_BY["by_proposal_voter"], 0, 0),
        # LIMIT
        ([""], "invalid-argument", 33, 0, 0),
        # ORDER TYPE
        ([""], 100, "invalid-argument", 0, 0),
        # ORDER DIRECTION
        ([""], 100, 33, "invalid-argument", 0),
        # STATUS
        ([""], 100, 33, 0, "invalid-argument"),
    ],
)
def test_list_proposal_votes_with_incorrect_type_of_argument(node, start, limit, order_by, order_direction, status):
    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.wallet_bridge.list_proposal_votes(start, limit, order_by, order_direction, status)
