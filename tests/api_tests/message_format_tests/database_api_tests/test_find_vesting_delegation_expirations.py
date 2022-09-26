import test_tools as tt

from ..local_tools import create_and_cancel_vesting_delegation, run_for
from ....local_tools import date_from_now


# This test cannot be performed on 5 million blocklog because it doesn't contain any vesting delegation expirations.
# See the readme.md file in this directory for further explanation.
@run_for('testnet')
def test_find_vesting_delegation_expirations_in_testnet(prepared_node):
    wallet = tt.Wallet(attach_to=prepared_node)
    wallet.create_account('alice', hives=tt.Asset.Test(100), vests=tt.Asset.Test(100))
    wallet.api.create_account('alice', 'bob', '{}')
    create_and_cancel_vesting_delegation(wallet, 'alice', 'bob')
    delegations = prepared_node.api.database.find_vesting_delegation_expirations(account='alice')['delegations']
    assert len(delegations) != 0


@run_for('mainnet_64m')
def test_find_vesting_delegation_expirations_in_mainnet_64m(prepared_node):
    delegations = prepared_node.api.database.find_vesting_delegation_expirations(account='temporary_name')['delegations']
    assert len(delegations) != 0
