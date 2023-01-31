import pytest

import test_tools as tt
from hive_local_tools import run_for
from hive_local_tools.api.message_format import as_string


@pytest.mark.parametrize(
    'reward_fund_name', [
        'alice',
        'bob',
    ]
)
@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_find_recurrent_transfers_with_correct_value(node, should_prepare, reward_fund_name):
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        create_accounts_and_make_recurrent_transfer(wallet, from_account='alice', to_account='bob')

    node.api.wallet_bridge.find_recurrent_transfers(reward_fund_name)


INCORRECT_VALUES = [
    'non-exist-acc',
    '',
    100,
    ["initminer"],
]


@pytest.mark.parametrize(
    'reward_fund_name', [
        *INCORRECT_VALUES,
        *as_string(INCORRECT_VALUES),
    ]
)
@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_find_recurrent_transfers_with_incorrect_value(node, reward_fund_name):
    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.wallet_bridge.find_recurrent_transfers(reward_fund_name)


@pytest.mark.parametrize(
    'reward_fund_name', [
        ['alice']
    ]
)
@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_find_recurrent_transfers_with_incorrect_type_of_argument(node, should_prepare, reward_fund_name):
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        create_accounts_and_make_recurrent_transfer(wallet, from_account='alice', to_account='bob')

    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.wallet_bridge.find_recurrent_transfers(reward_fund_name)


@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_find_recurrent_transfers_with_additional_argument(node, should_prepare):
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        create_accounts_and_make_recurrent_transfer(wallet, from_account='alice', to_account='bob')

    node.api.wallet_bridge.find_recurrent_transfers('alice', 'additional_argument')


def create_accounts_and_make_recurrent_transfer(wallet, from_account, to_account):
    with wallet.in_single_transaction():
        wallet.api.create_account('initminer', from_account, '{}')
        wallet.api.create_account('initminer', to_account, '{}')

    wallet.api.transfer_to_vesting('initminer', from_account, tt.Asset.Test(100))
    wallet.api.transfer('initminer', from_account, tt.Asset.Test(500), 'memo')
    wallet.api.recurrent_transfer(from_account, to_account, tt.Asset.Test(20), 'memo', 100, 10)
