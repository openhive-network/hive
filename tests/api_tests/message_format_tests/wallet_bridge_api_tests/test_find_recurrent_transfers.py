import pytest

import test_tools as tt

from .local_tools import as_string, run_for

# 'arcange' is an existing on mainnet account with recurring transfers enabled
ACCOUNT_NAMES = ['arcange', 'bob']


# Method find recurrent transfer its available since HF25, test on 5m node gives empty result
@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_find_recurrent_transfers_with_correct_value(prepared_node, should_prepare):
    if should_prepare:
        wallet = tt.Wallet(attach_to=prepared_node)
        create_accounts_and_make_recurrent_transfer(wallet, from_account=ACCOUNT_NAMES[0], to_account=ACCOUNT_NAMES[1])
    prepared_node.api.wallet_bridge.find_recurrent_transfers(ACCOUNT_NAMES[0])


INCORRECT_VALUES = [
    'non-exist-acc',
    '',
    100,
]


@pytest.mark.parametrize(
    'reward_fund_name', [
        *INCORRECT_VALUES,
        *as_string(INCORRECT_VALUES),
    ]
)
@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_find_recurrent_transfers_with_incorrect_value(prepared_node, reward_fund_name):
    with pytest.raises(tt.exceptions.CommunicationError):
        prepared_node.api.wallet_bridge.find_recurrent_transfers(reward_fund_name)


@pytest.mark.parametrize(
    'reward_fund_name', [
        [ACCOUNT_NAMES[0]]
    ]
)
@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_find_recurrent_transfers_with_incorrect_type_of_argument(prepared_node, should_prepare, reward_fund_name):
    if should_prepare:
        wallet = tt.Wallet(attach_to=prepared_node)
        create_accounts_and_make_recurrent_transfer(wallet, from_account=ACCOUNT_NAMES[0], to_account=ACCOUNT_NAMES[1])

    with pytest.raises(tt.exceptions.CommunicationError):
        prepared_node.api.wallet_bridge.find_recurrent_transfers(reward_fund_name)


@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_find_recurrent_transfers_with_additional_argument(prepared_node, should_prepare):
    if should_prepare:
        wallet = tt.Wallet(attach_to=prepared_node)
        create_accounts_and_make_recurrent_transfer(wallet, from_account=ACCOUNT_NAMES[0], to_account=ACCOUNT_NAMES[1])

    prepared_node.api.wallet_bridge.find_recurrent_transfers(ACCOUNT_NAMES[0], 'additional_argument')


def create_accounts_and_make_recurrent_transfer(wallet, from_account, to_account):
    with wallet.in_single_transaction():
        wallet.api.create_account('initminer', from_account, '{}')
        wallet.api.create_account('initminer', to_account, '{}')

    wallet.api.transfer_to_vesting('initminer', from_account, tt.Asset.Test(100))
    wallet.api.transfer('initminer', from_account, tt.Asset.Test(500), 'memo')
    wallet.api.recurrent_transfer(from_account, to_account, tt.Asset.Test(20), 'memo', 100, 10)
