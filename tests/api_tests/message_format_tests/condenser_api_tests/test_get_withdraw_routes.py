import test_tools as tt

from hive_local_tools import run_for


@run_for('testnet', 'mainnet_5m', 'live_mainnet')
def test_get_withdraw_routes(node, should_prepare):
    preparation_for_testnet_node(node, should_prepare)

    routes = node.api.condenser.get_withdraw_routes('blocktrades', 'all')
    assert len(routes) != 0


@run_for('testnet', 'mainnet_5m', 'live_mainnet')
def test_get_withdraw_routes_with_default_second_argument(node, should_prepare):
    preparation_for_testnet_node(node, should_prepare)
    {"jsonrpc": "2.0", "method": "database_api.list_withdraw_vesting_routes",
     "params": {"start": ["temp", ""], "limit": 10, "order": "by_withdraw_route"}, "id": 1}
    routes = node.api.condenser.get_withdraw_routes('blocktrades')
    assert len(routes) != 0


def preparation_for_testnet_node(node, should_prepare):
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        wallet.create_account('blocktrades', vests=tt.Asset.Test(100))

        wallet.api.create_account('blocktrades', 'alice', '{}')
        wallet.api.set_withdraw_vesting_route('blocktrades', 'alice', 15, True)
