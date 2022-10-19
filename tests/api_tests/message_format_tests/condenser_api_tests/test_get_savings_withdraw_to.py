import test_tools as tt

from ..local_tools import run_for, transfer_and_withdraw_from_savings
from ....local_tools import create_account_and_fund_it


# This test is not performed on 5 million and current block logs because they don't contain any savings withdrawals.
# See the readme.md file in this directory for further explanation.
@run_for('testnet')
def test_get_savings_withdraw_to(node, wallet):
    create_account_and_fund_it(wallet, 'alice', tests=tt.Asset.Test(100), vests=tt.Asset.Test(100))
    transfer_and_withdraw_from_savings(wallet, 'alice')
    withdrawals = node.api.condenser.get_savings_withdraw_to('alice')
    assert len(withdrawals) != 0
