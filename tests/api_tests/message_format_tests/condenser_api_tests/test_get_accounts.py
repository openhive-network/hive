import pytest

import test_tools as tt

from ..local_tools import run_for


@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_get_accounts(prepared_node, should_prepare):
    if should_prepare:
        wallet = tt.Wallet(attach_to=prepared_node)
        wallet.api.create_account('initminer', 'gtg', '{}')
    prepared_node.api.condenser.get_accounts(['gtg'])


@pytest.mark.parametrize(
    'account_key', [
        [['array-as-argument']],
        100,
        True,
        'incorrect_string_argument'
    ]
)
@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_get_accounts_with_incorrect_type_of_argument(prepared_node, account_key):
    with pytest.raises(tt.exceptions.CommunicationError):
        prepared_node.api.condenser.get_accounts(account_key)
