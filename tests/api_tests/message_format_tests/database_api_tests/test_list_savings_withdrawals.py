import test_tools as tt

from ..local_tools import transfer_and_withdraw_from_savings
from ....local_tools import create_account_and_fund_it, date_from_now


def test_list_savings_withdrawals(node, wallet):
    create_account_and_fund_it(wallet, 'alice', tests=tt.Asset.Test(100), vests=tt.Asset.Test(100))
    transfer_and_withdraw_from_savings(wallet, 'alice')
    withdrawals = node.api.database.list_savings_withdrawals(start=[date_from_now(weeks=-1), "alice", 0], limit=100,
                                                             order='by_complete_from_id')['withdrawals']
    assert len(withdrawals) != 0
