import test_tools as tt

from ..local_tools import run_for


@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_list_withdraw_vesting_routes(prepared_node, should_prepare):
    if should_prepare:
        wallet = tt.Wallet(attach_to=prepared_node)
        wallet.create_account('alice', vests=tt.Asset.Test(100))
        wallet.api.create_account('alice', 'bob', '{}')
        wallet.api.set_withdraw_vesting_route('alice', 'bob', 15, True)
    routes = prepared_node.api.database.list_withdraw_vesting_routes(start=['', ''], limit=100,
                                                                     order='by_withdraw_route')['routes']
    assert len(routes) != 0
