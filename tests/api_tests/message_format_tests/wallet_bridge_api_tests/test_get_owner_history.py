import pytest

import test_tools as tt

from .local_tools import as_string, run_for

ACCOUNT_NAMES = ['alice', 'aliks', 'artjoboy']  # accounts with owner history on testnet and mainnet

CORRECT_VALUES = [
    '',
    'non-exist-acc',
    *ACCOUNT_NAMES,
    100,
    True,
]


@pytest.mark.parametrize(
    'account_name', [
        *CORRECT_VALUES,
        *as_string(CORRECT_VALUES),
    ]
)
@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_get_owner_history_with_correct_value(prepared_node, should_prepare, account_name):
    if should_prepare:
        wallet = tt.Wallet(attach_to=prepared_node)
        create_and_update_account(wallet, account_name=ACCOUNT_NAMES[0])
    results = prepared_node.api.wallet_bridge.get_owner_history(account_name)
    if len(results['owner_auths']) != 0:
        assert len(results['owner_auths']) > 0


@pytest.mark.parametrize(
    'account_name', [
        [*ACCOUNT_NAMES]  # account name provided in array
    ]
)
@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_get_owner_history_with_incorrect_type_of_argument(prepared_node, should_prepare, account_name):
    if should_prepare:
        wallet = tt.Wallet(attach_to=prepared_node)
        create_and_update_account(wallet, account_name=ACCOUNT_NAMES[0])
    with pytest.raises(tt.exceptions.CommunicationError):
        prepared_node.api.wallet_bridge.get_owner_history(account_name)


def create_and_update_account(wallet, account_name):
    wallet.api.create_account('initminer', account_name, '{}')
    wallet.api.transfer_to_vesting('initminer', account_name, tt.Asset.Test(500))
    key = 'TST8grZpsMPnH7sxbMVZHWEu1D26F3GwLW1fYnZEuwzT4Rtd57AER'
    wallet.api.update_account(account_name, '{}', key, key, key, key)
