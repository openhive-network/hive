import test_tools as tt

from ....local_tools import create_account_and_fund_it, date_from_now, run_for


@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_get_trade_history(node, should_prepare):
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        create_account_and_fund_it(wallet, 'alice', tests=tt.Asset.Test(1000), vests=tt.Asset.Test(1000))
        create_account_and_fund_it(wallet, 'bob', tests=tt.Asset.Test(1000), vests=tt.Asset.Test(1000),
                                   tbds=tt.Asset.Tbd(1000))

        wallet.api.create_order('alice', 0, tt.Asset.Test(100), tt.Asset.Tbd(100), False, 3600)  # Sell 100 HIVE for 100 HBD
        wallet.api.create_order('bob', 0, tt.Asset.Tbd(100), tt.Asset.Test(100), False, 3600)  # Buy 100 HIVE for 100 HBD
    history = node.api.market_history.get_trade_history(start=date_from_now(weeks=-480),
                                                        end=date_from_now(weeks=0), limit=10)
    assert len(history) != 0
