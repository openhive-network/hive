import test_tools as tt

from hive_local_tools import run_for


@run_for('testnet', 'mainnet_5m', 'live_mainnet')
def test_find_accounts(node, should_prepare):
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        wallet.api.create_account('initminer', 'gtg', '{}')
    accounts = node.api.database.find_accounts(accounts=['gtg'])['accounts']
    assert len(accounts) != 0
