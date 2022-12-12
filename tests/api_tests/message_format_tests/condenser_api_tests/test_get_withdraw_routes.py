import test_tools as tt

from hive_local_tools import run_for


@run_for('testnet', 'mainnet_5m', 'live_mainnet')
def test_get_withdraw_routes(node, should_prepare):
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        wallet.create_account('blocktrades', vests=tt.Asset.Test(100))

        wallet.api.create_account('blocktrades', 'alice', '{}')
        wallet.api.set_withdraw_vesting_route('blocktrades', 'alice', 15, True)
    routes = node.api.condenser.get_withdraw_routes('blocktrades', 'all')
    assert len(routes) != 0
