import test_tools as tt

from ....local_tools import create_account_and_fund_it, run_for


@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_get_conversion_request(node, should_prepare):
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        create_account_and_fund_it(wallet,  'seanmclellan', tbds=tt.Asset.Tbd(100), vests=tt.Asset.Test(100))
        wallet.api.convert_hbd('seanmclellan', tt.Asset.Tbd(10))
    node.api.condenser.get_conversion_requests('seanmclellan')
