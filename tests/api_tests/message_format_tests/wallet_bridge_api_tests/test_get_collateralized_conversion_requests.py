import pytest

import test_tools as tt

from .local_tools import as_string, run_for


INCORRECT_VALUES = [
    'non-exist-acc',
    '',
    100,
    True,
]


@run_for('testnet')
def test_get_collateralized_conversion_requests_with_correct_value(prepared_node):
    wallet = tt.Wallet(attach_to=prepared_node)
    create_account_and_collateralize_conversion_request(wallet, account_name='alice')

    prepared_node.api.wallet_bridge.get_collateralized_conversion_requests('alice')


@pytest.mark.parametrize(
    'account_name', [
        *INCORRECT_VALUES,
        *as_string(INCORRECT_VALUES),
    ]
)
@run_for('testnet')
def test_get_collateralized_conversion_requests_with_incorrect_value(prepared_node, account_name):
    with pytest.raises(tt.exceptions.CommunicationError):
        prepared_node.api.wallet_bridge.get_conversion_requests(account_name)


@pytest.mark.parametrize(
    'account_name', [
        ['alice']
    ]
)
@run_for('testnet')
def test_get_collateralized_conversion_requests_with_incorrect_type_of_argument(prepared_node, account_name):
    with pytest.raises(tt.exceptions.CommunicationError):
        prepared_node.api.wallet_bridge.get_collateralized_conversion_requests(account_name)


@run_for('testnet')
def test_get_collateralized_conversion_requests_additional_argument(prepared_node):
    wallet = tt.Wallet(attach_to=prepared_node)
    create_account_and_collateralize_conversion_request(wallet, account_name='alice')

    prepared_node.api.wallet_bridge.get_collateralized_conversion_requests('alice', 'additional_argument')


def create_account_and_collateralize_conversion_request(wallet, account_name):
    wallet.api.create_account('initminer', account_name, '{}')
    wallet.api.transfer('initminer', account_name, tt.Asset.Test(100), 'memo')
    wallet.api.transfer_to_vesting('initminer', account_name, tt.Asset.Test(100))
    wallet.api.convert_hive_with_collateral(account_name, tt.Asset.Test(10))
