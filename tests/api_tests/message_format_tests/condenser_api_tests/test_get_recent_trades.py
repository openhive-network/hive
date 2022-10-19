import test_tools as tt

from ..local_tools import run_for
from ....local_tools import create_account_and_fund_it


@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_get_recent_trades(node, should_prepare):
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        create_account_and_fund_it(wallet, 'alice', tests=tt.Asset.Test(100), vests=tt.Asset.Test(100))
        create_account_and_fund_it(wallet, 'bob', tests=tt.Asset.Test(100), vests=tt.Asset.Test(100),
                                   tbds=tt.Asset.Tbd(100))

        wallet.api.create_order('alice', 0, tt.Asset.Test(100), tt.Asset.Tbd(100), False, 3600)  # Sell 100 HIVE for 100 HBD
        wallet.api.create_order('bob', 0, tt.Asset.Tbd(100), tt.Asset.Test(100), False, 3600)  # Buy 100 HIVE for 100 HBD

    trades = node.api.condenser.get_recent_trades(100)
    assert len(trades) != 0
