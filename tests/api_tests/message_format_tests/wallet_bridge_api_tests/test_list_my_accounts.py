import pytest

import test_tools as tt
from hive_local_tools import run_for
from hive_local_tools.api.message_format.wallet_bridge_api.constants import ACCOUNTS


@pytest.mark.parametrize(
    'accounts', [
        [ACCOUNTS[0]],
        ACCOUNTS,
    ]
)
@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_list_my_accounts_with_correct_value_in_testnet(node, should_prepare, accounts):
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        wallet.create_accounts(len(ACCOUNTS))

    memo_keys = []
    accounts = node.api.wallet_bridge.list_accounts(ACCOUNTS[0], 10)
    for account in accounts:
        memo_keys.append(node.api.wallet_bridge.get_account(account)['memo_key'])

    node.api.wallet_bridge.list_my_accounts(memo_keys)


@pytest.mark.parametrize(
    'account_key', [
        'TST6jTCfK3P5xYFJQvavAwz5M8KR6EW3TcmSesArj9LJVGAq85qor',  # non exist key with correct format
        'TST6jTCfK3P5xYFJQvavAwz5M8KR6EW3TcmSesArj9LJVGAq85qord',  # non exist key with too much signs
        'TS6jTCfK3P5xYFJQvavAwz5M8KR6EW3TcmSesArj9LJVGAq85qor',  # non exist key with less signs
        '',
        'true',
    ]
)
@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_list_my_accounts_with_incorrect_values(node, should_prepare, account_key):
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
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
@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_list_my_accounts_with_incorrect_type_of_argument(node, account_key):
    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.wallet_bridge.list_my_accounts([account_key])


@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_list_my_accounts_with_additional_argument(node, should_prepare):
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        wallet.create_accounts(len(ACCOUNTS))

    memo_keys = []
    accounts = node.api.wallet_bridge.list_accounts(ACCOUNTS[0], 10)
    for account in accounts:
        memo_keys.append(node.api.wallet_bridge.get_account(account)['memo_key'])

    node.api.wallet_bridge.list_my_accounts(memo_keys, 'additional_argument')


@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_list_my_accounts_with_missing_argument(node):
    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.wallet_bridge.list_my_accounts()
