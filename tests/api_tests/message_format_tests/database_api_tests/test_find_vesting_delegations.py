import test_tools as tt

from ..local_tools import run_for


# This test cannot be performed on 5 million blocklog because it doesn't contain any vesting delegations.
# See the readme.md file in this directory for further explanation.
@run_for('testnet', 'mainnet_64m')
def test_find_vesting_delegations(prepared_node, should_prepare):
    if should_prepare:
        wallet = tt.Wallet(attach_to=prepared_node)
        wallet.create_account('catharsis', hives=tt.Asset.Test(100), vests=tt.Asset.Test(100))
        wallet.api.create_account('catharsis', 'veshu1230', '{}')
        wallet.api.delegate_vesting_shares('catharsis', 'veshu1230', tt.Asset.Vest(5))
    account = prepared_node.api.database.list_vesting_delegations(start=['catharsis', 'veshu1230'], limit=100,
                                                                  order='by_delegation')['delegations'][0]['delegator']
    delegations = prepared_node.api.database.find_vesting_delegations(account=account)['delegations']
    assert len(delegations) != 0



