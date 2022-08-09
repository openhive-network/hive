import test_tools as tt

from ..local_tools import run_for
from ....local_tools import create_account_and_fund_it


@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_get_withdraw_routes(prepared_node, should_prepare):
    if should_prepare:
        wallet = tt.Wallet(attach_to=prepared_node)
        create_account_and_fund_it(wallet, 'blocktrades', vests=tt.Asset.Test(100))
        wallet.api.create_account('blocktrades', 'alice', '{}')
        wallet.api.set_withdraw_vesting_route('blocktrades', 'alice', 15, True)
    routes = prepared_node.api.condenser.get_withdraw_routes('blocktrades', 'all')
    assert len(routes) != 0
