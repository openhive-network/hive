import test_tools as tt

from ..local_tools import create_account_and_fund_it, run_for


@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_get_open_orders(node, should_prepare):
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        create_account_and_fund_it(wallet, 'abit', tests=tt.Asset.Test(100), vests=tt.Asset.Test(100))
        wallet.api.create_order('abit', 0, tt.Asset.Test(50), tt.Asset.Tbd(5), False, 3600)
    node.api.condenser.get_open_orders('abit')
