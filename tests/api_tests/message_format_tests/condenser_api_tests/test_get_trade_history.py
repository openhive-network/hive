import test_tools as tt

from ..local_tools import run_for


@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_get_trade_history(node, should_prepare):
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        wallet.create_account('alice', hives=tt.Asset.Test(100), vests=tt.Asset.Test(100))
        wallet.create_account('bob', hives=tt.Asset.Test(100), vests=tt.Asset.Test(100), hbds=tt.Asset.Tbd(100))

        wallet.api.create_order('alice', 0, tt.Asset.Test(100), tt.Asset.Tbd(100), False, 3600)  # Sell 100 HIVE for 100 HBD
        wallet.api.create_order('bob', 0, tt.Asset.Tbd(100), tt.Asset.Test(100), False, 3600)  # Buy 100 HIVE for 100 HBD
    history = node.api.condenser.get_trade_history(tt.Time.from_now(weeks=-480), tt.Time.now(), 10)
    assert len(history) != 0
