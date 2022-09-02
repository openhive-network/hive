import test_tools as tt

from ..local_tools import run_for


@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_get_collateralized_conversion_requests(prepared_node, should_prepare):
    if should_prepare:
        wallet = tt.Wallet(attach_to=prepared_node)
        wallet.api.convert_hive_with_collateral('initminer', tt.Asset.Test(10))
    # method introduced after HF25
    prepared_node.api.condenser.get_collateralized_conversion_requests('initminer')
