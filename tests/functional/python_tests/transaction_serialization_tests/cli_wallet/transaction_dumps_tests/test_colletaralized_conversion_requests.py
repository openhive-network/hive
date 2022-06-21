import test_tools as tt


def test_get_collateralized_conversion_requests(node, wallet):
    create_account_and_collateralize_conversion_request(wallet, account_name='alice')

    wallet.api.get_collateralized_conversion_requests('alice')


def create_account_and_collateralize_conversion_request(wallet, account_name): # czy przerzucać to globalnie??
    wallet.api.create_account('initminer', account_name, '{}')
    wallet.api.transfer('initminer', account_name, tt.Asset.Test(100), 'memo')
    wallet.api.transfer_to_vesting('initminer', account_name, tt.Asset.Test(100))
    wallet.api.convert_hive_with_collateral(account_name, tt.Asset.Test(10))