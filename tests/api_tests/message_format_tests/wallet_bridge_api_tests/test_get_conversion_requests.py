import pytest

import test_tools as tt

from .local_tools import as_string


INCORRECT_VALUES = [
    'non-exist-acc',
    '',
    100,
    True,
]


def test_get_conversion_requests_with_correct_value(node, wallet):
    create_account_with_converted_hbd(wallet, account_name='alice')

    node.api.wallet_bridge.get_conversion_requests('alice')


@pytest.mark.parametrize(
    'account_name', [
        *INCORRECT_VALUES,
        *as_string(INCORRECT_VALUES),
    ]
)
def test_get_conversion_requests_with_incorrect_value(node, account_name):
    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.wallet_bridge.get_conversion_requests(account_name)


@pytest.mark.parametrize(
    'account_name', [
        ['alice']
    ]
)
def test_get_conversion_requests_with_incorrect_type_of_argument(node, wallet, account_name):
    create_account_with_converted_hbd(wallet, account_name='alice')

    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.wallet_bridge.get_conversion_requests(account_name)


def test_get_conversion_requests_with_additional_argument(node, wallet):
    create_account_with_converted_hbd(wallet, account_name='alice')

    node.api.wallet_bridge.get_conversion_requests('alice', 'additional_argument')  # BUG1


def create_account_with_converted_hbd(wallet, account_name):
    wallet.api.create_account('initminer', account_name, '{}')
    wallet.api.transfer('initminer', account_name, tt.Asset.Test(100), 'memo')
    wallet.api.transfer_to_vesting('initminer', account_name, tt.Asset.Test(100))
    wallet.api.convert_hive_with_collateral(account_name, tt.Asset.Test(10))
    wallet.api.convert_hbd('alice', tt.Asset.Tbd(0.1))
    return account_name
