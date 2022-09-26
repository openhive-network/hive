import test_tools as tt

from ..local_tools import run_for


@run_for('testnet')
def test_find_change_recovery_account_requests_in_testnet(prepared_node):
    wallet = tt.Wallet(attach_to=prepared_node)
    wallet.create_account('alice', hives=tt.Asset.Test(100), vests=tt.Asset.Test(100))
    wallet.api.change_recovery_account('alice', 'steem.dao')
    requests = prepared_node.api.database.find_change_recovery_account_requests(accounts=['alice'])['requests']
    assert len(requests) != 0


@run_for('mainnet_5m', 'mainnet_64m')
def test_find_change_recovery_account_requests_in_mainnet(prepared_node):
    requests = prepared_node.api.database.find_change_recovery_account_requests(accounts=['temporary_name'])['requests']
    assert len(requests) != 0
