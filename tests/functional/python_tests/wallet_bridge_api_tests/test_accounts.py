import pytest

from test_tools import exceptions
from validate_message import validate_message
import schemas

ACCOUNTS = [f'account-{i}' for i in range(10)]

# TODO BUG LIST!
"""
1. Problem with running wallet_bridge_api.get_account_history with incorrect value ( putting negative value to
   from_ argument). I think, in this situation program should throw an exception.  (# BUG1) 
   More information about get_account_history is in this place: 
        https://developers.hive.io/apidefinitions/#account_history_api.get_account_history

2. Problem with running wallet_bridge_api.get_account_history with incorrect argument type 
   (putting bool as limit ). I think, in this situation program should throw an exception.  (# BUG2) 

3. Problem with running wallet_bridge_api.get_accounts with incorrect argument type 
   (putting array as argument). I think, in this situation program should throw an exception.  (# BUG3) 

4. Problem with running wallet_bridge_api.list_accounts with incorrect argument  type 
   (putting bool as limit argument). I think, in this situation program should throw an exception.  (# BUG4) 
"""


@pytest.mark.parametrize(
    'account', [
        ACCOUNTS[0],
        'non-exist-acc',
        '',
    ]
)
def test_get_account_with_correct_value(node, wallet, account):
    # TODO Add pattern test
    wallet.create_accounts(len(ACCOUNTS))
    response = node.api.wallet_bridge.get_account(account)


@pytest.mark.parametrize(
    'account', [
        ['example_array'],
        100,
        True,
    ]
)
def test_get_account_with_incorrect_type_of_argument(node, account):
    with pytest.raises(exceptions.CommunicationError):
        node.api.wallet_bridge.get_account(account)


@pytest.mark.parametrize(
    'account, from_, limit', [
        # ACCOUNT
        (ACCOUNTS[0], -1, 1000),
        ('non-exist-acc', -1, 1000),  # To miejsce mi się nie podoba, tutaj konto jest kluczowe
        ('', -1, 1000),  # To miejsce mi się nie podoba, tutaj konto jest kluczowe

        # FROM
        (ACCOUNTS[0], 4294967295, 1000),   # maximal value of uint32
        (ACCOUNTS[0], 2, 1),
        (ACCOUNTS[0], -1, 1000),

        # #LIMIT
        (ACCOUNTS[0], -1, 0),
        (ACCOUNTS[0], -1, 1000),
    ]
)
def test_get_account_history_with_correct_value(node, wallet, account, from_, limit):
    # TODO Add pattern test
    wallet.create_accounts(len(ACCOUNTS))

    node.wait_number_of_blocks(21)  # wait 21 block to appear tranastions in 'get account history'
    response = node.api.wallet_bridge.get_account_history(account, from_, limit)


@pytest.mark.parametrize(
    'account, from_, limit', [
        # FROM
        (ACCOUNTS[5], 4294967296, 1000),  #   # maximal value of uint32 + 1
        # (ACCOUNTS[5], -2, 1000),  # BUG1

        # LIMIT
        (ACCOUNTS[5], -1, -1),
        (ACCOUNTS[5], 1, 0),
        (ACCOUNTS[5], -1, 1001),
    ]
)
def test_get_account_history_with_incorrect_value(node, account, from_, limit):
    with pytest.raises(exceptions.CommunicationError):
        node.api.wallet_bridge.get_account_history(account, from_, limit)


@pytest.mark.parametrize(
    'account, from_, limit', [
        # ACCOUNT
        (100, -1, 1000),
        (True, -1, 1000),
        (['example_array'], -1, 1000),

        # FROM
        (ACCOUNTS[5], 'example-string-argument', 1000),
        (ACCOUNTS[5], True, 1000),
        (ACCOUNTS[5], [-1], 1000),

        # LIMIT
        (ACCOUNTS[5], -1, 'example-string-argument'),
        # (ACCOUNTS[5], -1, True),  # BUG2
        (ACCOUNTS[5], -1, [1000]),
    ]
)
def test_get_account_history_with_incorrect_type_of_argument(node, wallet, account, from_, limit):
    wallet.create_accounts(len(ACCOUNTS))

    with pytest.raises(exceptions.CommunicationError):
        node.api.wallet_bridge.get_account_history(account, from_, limit)


@pytest.mark.parametrize(
    'account', [
        ACCOUNTS,
        [ACCOUNTS[0]],
        ['non-exist-acc'],  # To miejsce mi się nie podoba, tutaj konto jest kluczowe
        [''],  # To miejsce mi się nie podoba, tutaj konto jest kluczowe
    ]
)
def test_get_accounts_with_correct_value(node, wallet, account):
    # TODO Add pattern test
    wallet.create_accounts(len(ACCOUNTS))

    response = node.api.wallet_bridge.get_accounts(account)


@pytest.mark.parametrize(
    'account_key', [
        # ['example-array'],  # BUG3
        100,
        True,
        'incorrect_string_argument'
    ]
)
def test_get_accounts_with_incorrect_type_of_argument(node, account_key):
    with pytest.raises(exceptions.CommunicationError):
        node.api.wallet_bridge.get_accounts(account_key)


@pytest.mark.parametrize(
    'lowerbound_account, limit', [
        # LOWERBOUND ACCOUNT
        (ACCOUNTS[0], 100),
        ('non-exist-acc', 100),
        ('', 100),

        # LIMIT
        (ACCOUNTS[0], 0),
        (ACCOUNTS[0], 1000),
    ]
)
def test_list_accounts_with_correct_value(node, wallet, lowerbound_account, limit):
    # TODO Add pattern test
    wallet.create_accounts(len(ACCOUNTS))

    response = node.api.wallet_bridge.list_accounts(lowerbound_account, limit)


@pytest.mark.parametrize(
    'lowerbound_account, limit', [
        # LIMIT
        (ACCOUNTS[0], -1),
        (ACCOUNTS[0], 1001),
    ]
)
def test_list_accounts_with_incorrect_values(node, wallet, lowerbound_account, limit):
    wallet.create_accounts(len(ACCOUNTS))

    with pytest.raises(exceptions.CommunicationError):
        node.api.wallet_bridge.list_accounts(lowerbound_account, limit)


@pytest.mark.parametrize(
    'lowerbound_account, limit', [
        # LOWERBOUND ACCOUNT
        (True, 100),
        (100, 100),
        (['example-array'], 100),

        # LIMIT
        # (ACCOUNTS[0], True),  # BUG4
        (ACCOUNTS[0], 'incorrect_string_argument'),
        (ACCOUNTS[0], [100]),
    ]
)
def test_list_accounts_with_incorrect_type_of_argument(node, lowerbound_account, limit):
    with pytest.raises(exceptions.CommunicationError):
        node.api.wallet_bridge.list_accounts(lowerbound_account, limit)


@pytest.mark.parametrize(
    'accounts', [
        [ACCOUNTS[0]],
        ACCOUNTS,
    ]
)
def test_list_my_accounts_with_correct_value(node, wallet, accounts):
    # TODO Add pattern test
    wallet.create_accounts(len(ACCOUNTS))
    memo_keys = []
    for account in accounts:
        memo_keys.append(wallet.api.get_account(account)['memo_key'])

    response = node.api.wallet_bridge.list_my_accounts(memo_keys)


@pytest.mark.parametrize(
    'account_key', [
        'TST6jTCfK3P5xYFJQvavAwz5M8KR6EW3TcmSesArj9LJVGAq85qor',  # non exist key with correct format
        'TST6jTCfK3P5xYFJQvavAwz5M8KR6EW3TcmSesArj9LJVGAq85qord',  # non exist key with too much signs
        'TS6jTCfK3P5xYFJQvavAwz5M8KR6EW3TcmSesArj9LJVGAq85qor',  # non exist key with less signs
        '',
    ]
)
def test_list_my_accounts_with_incorrect_values(node, wallet, account_key):
    wallet.create_accounts(len(ACCOUNTS))

    with pytest.raises(exceptions.CommunicationError):
        node.api.wallet_bridge.list_my_accounts([account_key])


@pytest.mark.parametrize(
    'account_key', [
        ['example-array'],
        100,
        True,
        'incorrect_string_argument'
    ]
)
def test_list_my_accounts_with_incorrect_type_of_argument(node, account_key):
    with pytest.raises(exceptions.CommunicationError):
        node.api.wallet_bridge.list_my_accounts([account_key])


def test_list_my_accounts_with_additional_argument(node, wallet):
    wallet.create_accounts(len(ACCOUNTS))
    memo_keys = []
    for account in ACCOUNTS:
        memo_keys.append(wallet.api.get_account(account)['memo_key'])

    with pytest.raises(exceptions.CommunicationError):
        node.api.wallet_bridge.list_my_accounts(memo_keys, 'additional_argument')


def test_list_my_accounts_with_missing_argument(node, wallet):
    with pytest.raises(exceptions.CommunicationError):
        node.api.wallet_bridge.list_my_accounts()
