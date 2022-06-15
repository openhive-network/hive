import pytest

import test_tools as tt

from .local_tools import run_for

TESTNET_ACCOUNTS = [f'account-{i}' for i in range(10)]


@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_list_my_accounts_with_correct_value(prepared_node, should_prepare):
    if should_prepare:
        wallet = tt.Wallet(attach_to=prepared_node)
        wallet.create_accounts(len(TESTNET_ACCOUNTS))
    memo_keys = []
    accounts = prepared_node.api.wallet_bridge.list_accounts('', 100, 'by_name')
    for account in accounts:
        if prepared_node.api.wallet_bridge.get_account(account)['memo_key'] is not None:
            memo_keys.append(prepared_node.api.wallet_bridge.get_account(account)['memo_key'])
    result = prepared_node.api.wallet_bridge.list_my_accounts(memo_keys)
    assert len(result) > 0


@pytest.mark.parametrize(
    'account_key', [
        'TST6jTCfK3P5xYFJQvavAwz5M8KR6EW3TcmSesArj9LJVGAq85qor',  # non exist key with correct format
        'TST6jTCfK3P5xYFJQvavAwz5M8KR6EW3TcmSesArj9LJVGAq85qord',  # non exist key with too much signs
        'TS6jTCfK3P5xYFJQvavAwz5M8KR6EW3TcmSesArj9LJVGAq85qor',  # non exist key with less signs
        '',
        'true',
    ]
)
@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_list_my_accounts_with_incorrect_values(prepared_node, should_prepare, account_key):
    if should_prepare:
        wallet = tt.Wallet(attach_to=prepared_node)
        wallet.create_accounts(len(TESTNET_ACCOUNTS))

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
@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_list_my_accounts_with_incorrect_type_of_argument(prepared_node, account_key):
    with pytest.raises(tt.exceptions.CommunicationError):
        prepared_node.api.wallet_bridge.list_my_accounts([account_key])


@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_list_my_accounts_with_additional_argument(prepared_node, should_prepare):
    if should_prepare:
        wallet = tt.Wallet(attach_to=prepared_node)
        wallet.create_accounts(len(TESTNET_ACCOUNTS))
    memo_keys = []
    accounts = prepared_node.api.wallet_bridge.list_accounts('', 100, 'by_name')
    for account in accounts:
        if prepared_node.api.wallet_bridge.get_account(account)['memo_key'] is not None:
            memo_keys.append(prepared_node.api.wallet_bridge.get_account(account)['memo_key'])

    result = prepared_node.api.wallet_bridge.list_my_accounts(memo_keys, 'additional_argument')
    assert len(result) > 0


@run_for('testnet',  'mainnet_5m', 'mainnet_64m')
def test_list_my_accounts_with_missing_argument(prepared_node):
    with pytest.raises(tt.exceptions.CommunicationError):
        prepared_node.api.wallet_bridge.list_my_accounts()
