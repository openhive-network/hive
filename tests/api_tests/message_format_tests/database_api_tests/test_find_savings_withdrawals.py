import test_tools as tt

from ..local_tools import transfer_and_withdraw_from_savings, run_for
from ....local_tools import date_from_now


# This test cannot be performed on 5 million blocklog because it doesn't contain any savings withdrawals.
# See the readme.md file in this directory for further explanation.
@run_for('testnet')
def test_find_savings_withdrawals_in_testnet(prepared_node):
    wallet = tt.Wallet(attach_to=prepared_node)
    wallet.create_account('alice', hives=tt.Asset.Test(100), vests=tt.Asset.Test(100))
    transfer_and_withdraw_from_savings(wallet, 'alice')
    withdrawals = prepared_node.api.database.find_savings_withdrawals(account='alice')['withdrawals']
    assert len(withdrawals) != 0


@run_for('mainnet_64m')
def test_find_savings_withdrawals_in_mainnet_64m(prepared_node):
    withdrawals = prepared_node.api.database.find_savings_withdrawals(account='temporary_name')['withdrawals']
    assert len(withdrawals) != 0
