import test_tools as tt

from ..local_tools import run_for
from ....local_tools import create_account_and_fund_it


# This test is not performed on 5 million block log because it doesn't contain any vesting delegations.
# See the readme.md file in this directory for further explanation.
@run_for('testnet', 'mainnet_64m')
def test_get_vesting_delegations(node, should_prepare):
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        create_account_and_fund_it(wallet, 'coar', tests=tt.Asset.Test(100), vests=tt.Asset.Test(100))
        wallet.api.create_account('coar', 'tipu', '{}')
        wallet.api.delegate_vesting_shares('coar', 'tipu', tt.Asset.Vest(5))
    delegations = node.api.condenser.get_vesting_delegations('coar', '', 100)
    assert len(delegations) != 0
