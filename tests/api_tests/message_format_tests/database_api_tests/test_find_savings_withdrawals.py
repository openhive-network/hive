import test_tools as tt

from ..local_tools import transfer_and_withdraw_from_savings
from ....local_tools import create_account_and_fund_it


def test_find_savings_withdrawals(node, wallet):
    create_account_and_fund_it(wallet, 'alice', tests=tt.Asset.Test(100), vests=tt.Asset.Test(100))
    transfer_and_withdraw_from_savings(wallet, 'alice')
    withdrawals = node.api.database.find_savings_withdrawals(account='alice')['withdrawals']
    assert len(withdrawals) != 0
