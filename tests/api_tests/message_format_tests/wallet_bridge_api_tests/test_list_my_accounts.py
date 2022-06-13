import pytest

import test_tools as tt


ACCOUNTS = [f'account-{i}' for i in range(10)]


@pytest.mark.parametrize(
    'accounts', [
        [ACCOUNTS[0]],
        ACCOUNTS,
    ]
)
@pytest.mark.testnet
def test_list_my_accounts_with_correct_value(node, wallet, accounts):
    wallet.create_accounts(len(ACCOUNTS))
    memo_keys = []
    for account in accounts:
        memo_keys.append(wallet.api.get_account(account)['memo_key'])

    node.api.wallet_bridge.list_my_accounts(memo_keys)


@pytest.mark.remote_node_5m
def test_list_my_accounts_with_correct_value_5m(node5m):
    memo_key = node5m.api.wallet_bridge.get_account('gtg')['memo_key']
    node5m.api.wallet_bridge.list_my_accounts([memo_key])


@pytest.mark.remote_node_64m
def test_list_my_accounts_with_correct_value_64m(node64m):
    memo_key = node64m.api.wallet_bridge.get_account('gtg')['memo_key']
    node64m.api.wallet_bridge.list_my_accounts([memo_key])


@pytest.mark.parametrize(
    'account_key', [
        'TST6jTCfK3P5xYFJQvavAwz5M8KR6EW3TcmSesArj9LJVGAq85qor',  # non exist key with correct format
        'TST6jTCfK3P5xYFJQvavAwz5M8KR6EW3TcmSesArj9LJVGAq85qord',  # non exist key with too much signs
        'TS6jTCfK3P5xYFJQvavAwz5M8KR6EW3TcmSesArj9LJVGAq85qor',  # non exist key with less signs
        '',
        'true',
    ]
)
@pytest.mark.testnet
def test_list_my_accounts_with_incorrect_values(node, wallet, account_key):
    wallet.create_accounts(len(ACCOUNTS))

    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.wallet_bridge.list_my_accounts([account_key])


@pytest.mark.parametrize(
    'account_key', [
        ['example-array'],
        100,
        True,
        'incorrect_string_argument'
    ]
)
@pytest.mark.testnet
def test_list_my_accounts_with_incorrect_type_of_argument(node, account_key):
    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.wallet_bridge.list_my_accounts([account_key])


@pytest.mark.testnet
def test_list_my_accounts_with_additional_argument(node, wallet):
    wallet.create_accounts(len(ACCOUNTS))
    memo_keys = []
    for account in ACCOUNTS:
        memo_keys.append(wallet.api.get_account(account)['memo_key'])

    node.api.wallet_bridge.list_my_accounts(memo_keys, 'additional_argument')


@pytest.mark.testnet
def test_list_my_accounts_with_missing_argument(node):
    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.wallet_bridge.list_my_accounts()
