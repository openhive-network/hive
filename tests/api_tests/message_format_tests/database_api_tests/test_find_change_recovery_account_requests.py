import test_tools as tt

from hive_local_tools import run_for


@run_for('testnet')
def test_find_change_recovery_account_requests_in_testnet(node):
    wallet = tt.Wallet(attach_to=node)
    wallet.create_account('alice', hives=tt.Asset.Test(100), vests=tt.Asset.Test(100))
    wallet.api.change_recovery_account('alice', 'steem.dao')
    requests = node.api.database.find_change_recovery_account_requests(accounts=['alice'])['requests']
    assert len(requests) != 0


@run_for('mainnet_5m', 'live_mainnet')
def test_find_change_recovery_account_requests_in_mainnet(node):
    account = node.api.database.list_change_recovery_account_requests(start='', limit=100, order='by_account')['requests'][0]['account_to_recover']
    requests = node.api.database.find_change_recovery_account_requests(accounts=[account])['requests']
    assert len(requests) != 0
