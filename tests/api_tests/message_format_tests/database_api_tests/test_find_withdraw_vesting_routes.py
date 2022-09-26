import test_tools as tt

from ..local_tools import run_for


@run_for('testnet')
def test_find_withdraw_vesting_routes_in_testnet(prepared_node):
    wallet = tt.Wallet(attach_to=prepared_node)
    wallet.create_account('alice', hives=tt.Asset.Test(100), vests=tt.Asset.Test(100))
    wallet.api.create_account('alice', 'bob', '{}')
    wallet.api.set_withdraw_vesting_route('alice', 'bob', 15, True)
    routes = prepared_node.api.database.find_withdraw_vesting_routes(account='bob', order='by_destination')['routes']
    assert len(routes) != 0


@run_for('mainnet_5m', 'mainnet_64m')
def test_find_withdraw_vesting_routes_in_mainnet(prepared_node):
    routes = prepared_node.api.database.find_withdraw_vesting_routes(account='temporary_name', order='by_destination')['routes']
    assert len(routes) != 0
