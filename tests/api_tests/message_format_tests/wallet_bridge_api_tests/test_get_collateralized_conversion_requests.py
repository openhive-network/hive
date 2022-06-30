# Testing only on testnet and mainnet 64 millionth node, this method was added in HF25
import pytest

import test_tools as tt

from .local_tools import as_string, run_for

ACCOUNT = 'gtg'

INCORRECT_VALUES = [
    'non-exist-acc',
    '',
    100,
    # True,  TODO: 'true' its existing account on mainnet
]


@run_for('testnet', 'mainnet_64m')
def test_get_collateralized_conversion_requests_with_correct_value(prepared_node, should_prepare):
    if should_prepare:
        wallet = tt.Wallet(attach_to=prepared_node)
        create_account_and_collateralize_conversion_request(wallet, account_name=ACCOUNT)

    prepared_node.api.wallet_bridge.get_collateralized_conversion_requests(ACCOUNT)


@pytest.mark.parametrize(
    'account_name', [
        *INCORRECT_VALUES,
        *as_string(INCORRECT_VALUES),
    ]
)
@run_for('testnet', 'mainnet_64m')
def test_get_collateralized_conversion_requests_with_incorrect_value(prepared_node, account_name):
    with pytest.raises(tt.exceptions.CommunicationError):
        prepared_node.api.wallet_bridge.get_conversion_requests(account_name)


@pytest.mark.parametrize(
    'account_name', [
        [ACCOUNT]
    ]
)
@run_for('testnet', 'mainnet_64m')
def test_get_collateralized_conversion_requests_with_incorrect_type_of_argument(prepared_node, account_name):
    with pytest.raises(tt.exceptions.CommunicationError):
        prepared_node.api.wallet_bridge.get_collateralized_conversion_requests(account_name)


@run_for('testnet', 'mainnet_64m')
def test_get_collateralized_conversion_requests_additional_argument(prepared_node, should_prepare):
    if should_prepare:
        wallet = tt.Wallet(attach_to=prepared_node)
        create_account_and_collateralize_conversion_request(wallet, account_name=ACCOUNT)

    prepared_node.api.wallet_bridge.get_collateralized_conversion_requests(ACCOUNT, 'additional_argument')


def create_account_and_collateralize_conversion_request(wallet, account_name):
    wallet.api.create_account('initminer', account_name, '{}')
    wallet.api.transfer('initminer', account_name, tt.Asset.Test(100), 'memo')
    wallet.api.transfer_to_vesting('initminer', account_name, tt.Asset.Test(100))
    wallet.api.convert_hive_with_collateral(account_name, tt.Asset.Test(10))
