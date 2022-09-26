import test_tools as tt

from ..local_tools import run_for


# This test cannot be performed on 5 million blocklog because it doesn't contain any vesting delegations.
# See the readme.md file in this directory for further explanation.
@run_for('testnet')
def test_find_vesting_delegations_in_testnet(prepared_node):
    wallet = tt.Wallet(attach_to=prepared_node)
    wallet.create_account('alice', hives=tt.Asset.Test(100), vests=tt.Asset.Test(100))
    wallet.api.create_account('alice', 'bob', '{}')
    wallet.api.delegate_vesting_shares('alice', 'bob', tt.Asset.Vest(5))
    delegations = prepared_node.api.database.find_vesting_delegations(account='alice')['delegations']
    assert len(delegations) != 0


@run_for('mainnet_64m')
def test_find_vesting_delegations_in_mainnet_64m(prepared_node):
    delegations = prepared_node.api.database.find_vesting_delegations(account='temporary_name')['delegations']
    assert len(delegations) != 0
