import test_tools as tt

from ..local_tools import run_for, date_from_now
from ....local_tools import create_account_and_fund_it


@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_get_market_history(prepared_node, should_prepare):
    if should_prepare:
        wallet = tt.Wallet(attach_to=prepared_node)
        create_account_and_fund_it(wallet, 'alice', tests=tt.Asset.Test(100), vests=tt.Asset.Test(100))
        wallet.api.create_order('alice', 0, tt.Asset.Test(50), tt.Asset.Tbd(5), False, 3600)
        wallet.api.create_order('initminer', 1, tt.Asset.Tbd(50), tt.Asset.Test(100), False, 3600)
    history = prepared_node.api.market_history.get_market_history(bucket_seconds=3600, start=date_from_now(weeks=-100),
                                                                  end=date_from_now(weeks=1))
    assert len(history) != 0
