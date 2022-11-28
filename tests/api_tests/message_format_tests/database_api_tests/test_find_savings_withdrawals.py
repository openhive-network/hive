import test_tools as tt

from hive_local_tools import run_for
from hive_local_tools.api.message_format import transfer_and_withdraw_from_savings


# This test cannot be performed on 5 million blocklog because it doesn't contain any savings withdrawals.
# See the readme.md file in this directory for further explanation.
@run_for('testnet', 'live_mainnet')
def test_find_savings_withdrawals(node, should_prepare):
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        wallet.create_account('alice', hives=tt.Asset.Test(100), vests=tt.Asset.Test(100))
        transfer_and_withdraw_from_savings(wallet, 'alice')
    account = node.api.database.list_savings_withdrawals(start=[tt.Time.from_now(weeks=-100), '', 0],
                                                         limit=100,
                                                         order='by_complete_from_id')['withdrawals'][0]['to']
    withdrawals = node.api.database.find_savings_withdrawals(account=account)['withdrawals']
    assert len(withdrawals) != 0
