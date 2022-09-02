import test_tools as tt

from ..local_tools import run_for
from ....local_tools import create_account_and_fund_it


@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_get_order_book(prepared_node, should_prepare):
    if should_prepare:
        wallet = tt.Wallet(attach_to=prepared_node)
        create_account_and_fund_it(wallet, 'alice', tests=tt.Asset.Test(100), vests=tt.Asset.Test(100))
        create_account_and_fund_it(wallet, 'bob', tests=tt.Asset.Test(100), vests=tt.Asset.Test(100),
                                   tbds=tt.Asset.Tbd(100))

        wallet.api.create_order('alice', 1, tt.Asset.Test(100), tt.Asset.Tbd(50), False, 3600)
        wallet.api.create_order('bob', 2, tt.Asset.Tbd(20), tt.Asset.Test(100), False, 3600)
    response = prepared_node.api.market_history.get_order_book(limit=100)
    assert len(response['bids']) != 0
    assert len(response['asks']) != 0
