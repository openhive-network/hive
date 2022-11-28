import test_tools as tt

from hive_local_tools import run_for


@run_for('testnet', 'mainnet_5m', 'live_mainnet')
def test_find_withdraw_vesting_routes(node, should_prepare):
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        wallet.create_account('alice', hives=tt.Asset.Test(100), vests=tt.Asset.Test(100))
        wallet.api.create_account('alice', 'bob', '{}')
        wallet.api.set_withdraw_vesting_route('alice', 'bob', 15, True)
    account = node.api.database.list_withdraw_vesting_routes(start=['', ''], limit=100,
                                                                      order='by_withdraw_route')['routes'][0][
        'to_account']
    routes = node.api.database.find_withdraw_vesting_routes(account=account, order='by_destination')['routes']
    assert len(routes) != 0
