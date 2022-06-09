import pytest

import test_tools as tt

from .local_tools import run_for

ACCOUNTS = [f'account-{i}' for i in range(10)]


@pytest.mark.parametrize(
    'accounts', [
        [ACCOUNTS[0]],
        ACCOUNTS,
    ]
)
@run_for('testnet')
def test_list_my_accounts_with_correct_value_in_testnet(prepared_node, accounts):
    wallet = tt.Wallet(attach_to=prepared_node)
    wallet.create_accounts(len(ACCOUNTS))
    memo_keys = []
    for account in accounts:
        memo_keys.append(wallet.api.get_account(account)['memo_key'])

    prepared_node.api.wallet_bridge.list_my_accounts(memo_keys)


@pytest.mark.parametrize(
    'account_key', [
        'TST6jTCfK3P5xYFJQvavAwz5M8KR6EW3TcmSesArj9LJVGAq85qor',  # non exist key with correct format
        'TST6jTCfK3P5xYFJQvavAwz5M8KR6EW3TcmSesArj9LJVGAq85qord',  # non exist key with too much signs
        'TS6jTCfK3P5xYFJQvavAwz5M8KR6EW3TcmSesArj9LJVGAq85qor',  # non exist key with less signs
        '',
        'true',
    ]
)
@run_for('testnet')
def test_list_my_accounts_with_incorrect_values(prepared_node, account_key):
    wallet = tt.Wallet(attach_to=prepared_node)
    wallet.create_accounts(len(ACCOUNTS))

    with pytest.raises(tt.exceptions.CommunicationError):
        prepared_node.api.wallet_bridge.list_my_accounts([account_key])


@pytest.mark.parametrize(
    'account_key', [
        ['example-array'],
        100,
        True,
        'incorrect_string_argument'
    ]
)
@run_for('testnet')
def test_list_my_accounts_with_incorrect_type_of_argument(prepared_node, account_key):
    with pytest.raises(tt.exceptions.CommunicationError):
        prepared_node.api.wallet_bridge.list_my_accounts([account_key])


@run_for('testnet')
def test_list_my_accounts_with_additional_argument(prepared_node):
    wallet = tt.Wallet(attach_to=prepared_node)
    wallet.create_accounts(len(ACCOUNTS))
    memo_keys = []
    for account in ACCOUNTS:
        memo_keys.append(wallet.api.get_account(account)['memo_key'])

    prepared_node.api.wallet_bridge.list_my_accounts(memo_keys, 'additional_argument')


@run_for('testnet')
def test_list_my_accounts_with_missing_argument(prepared_node):
    with pytest.raises(tt.exceptions.CommunicationError):
        prepared_node.api.wallet_bridge.list_my_accounts()
