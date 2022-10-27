import test_tools as tt

from ..local_tools import transfer_and_withdraw_from_savings, run_for
from ....local_tools import date_from_now


# This test cannot be performed on 5 million blocklog because it doesn't contain any savings withdrawals.
# See the readme.md file in this directory for further explanation.
@run_for('testnet', 'mainnet_64m')
def test_list_savings_withdrawals(prepared_node, should_prepare):
    if should_prepare:
        wallet = tt.Wallet(attach_to=prepared_node)
        wallet.create_account('alice', hives=tt.Asset.Test(100), vests=tt.Asset.Test(100))
        transfer_and_withdraw_from_savings(wallet, 'alice')
    withdrawals = prepared_node.api.database.list_savings_withdrawals(start=[date_from_now(weeks=-100), '', 0],
                                                                      limit=100,
                                                                      order='by_complete_from_id')['withdrawals']
    assert len(withdrawals) != 0
