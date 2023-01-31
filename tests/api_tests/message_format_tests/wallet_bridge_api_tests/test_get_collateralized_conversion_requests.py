import pytest

import test_tools as tt
from hive_local_tools import run_for
from hive_local_tools.api.message_format import as_string

INCORRECT_VALUES = [
    'non-exist-acc',
    '',
    100,
    ["initminer"]
]


@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_get_collateralized_conversion_requests_with_correct_value(node, should_prepare):
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        create_account_and_collateralize_conversion_request(wallet, account_name='alice')

    node.api.wallet_bridge.get_collateralized_conversion_requests('alice')


@pytest.mark.parametrize(
    'account_name', [
        *INCORRECT_VALUES,
        *as_string(INCORRECT_VALUES),
    ]
)
@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_get_collateralized_conversion_requests_with_incorrect_value(node, account_name):
    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.wallet_bridge.get_conversion_requests(account_name)


@pytest.mark.parametrize(
    'account_name', [
        ['alice']
    ]
)
@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_get_collateralized_conversion_requests_with_incorrect_type_of_argument(node, account_name):
    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.wallet_bridge.get_collateralized_conversion_requests(account_name)


@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_get_collateralized_conversion_requests_additional_argument(node, should_prepare):
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        create_account_and_collateralize_conversion_request(wallet, account_name='alice')

    node.api.wallet_bridge.get_collateralized_conversion_requests('alice', 'additional_argument')


def create_account_and_collateralize_conversion_request(wallet, account_name):
    wallet.api.create_account('initminer', account_name, '{}')
    wallet.api.transfer('initminer', account_name, tt.Asset.Test(100), 'memo')
    wallet.api.transfer_to_vesting('initminer', account_name, tt.Asset.Test(100))
    wallet.api.convert_hive_with_collateral(account_name, tt.Asset.Test(10))
