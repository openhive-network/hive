import re

import test_tools as tt


def test_conversion(wallet):
    response = wallet.api.create_account('initminer', 'alice', '{}')

    response = wallet.api.transfer('initminer', 'alice', tt.Asset.Test(200), 'avocado')

    wallet.api.transfer_to_vesting('initminer', 'alice', tt.Asset.Test(500))

    response = wallet.api.get_account('alice')

    assert response['balance'] == tt.Asset.Test(200)
    assert response['hbd_balance'] == tt.Asset.Tbd(0)

    response = wallet.api.convert_hive_with_collateral('alice', tt.Asset.Test(4))

    _ops = response['operations']
    assert _ops[0][1]['amount'] == tt.Asset.Test(4)

    response = wallet.api.get_account('alice')

    assert response['balance'] == tt.Asset.Test(196)
    assert response['hbd_balance'] == tt.Asset.Tbd(1.904)

    response = wallet.api.get_collateralized_conversion_requests('alice')

    _request = response[0]
    assert _request['collateral_amount'] == tt.Asset.Test(4)
    assert _request['converted_amount'] == tt.Asset.Tbd(1.904)

    assert is_valid_asset(wallet.api.estimate_hive_collateral(tt.Asset.Test(4)))

    wallet.api.convert_hbd('alice', tt.Asset.Tbd(0.5))

    _result = wallet.api.get_account('alice')

    #'balance' is still the same, because request of conversion will be done after 3.5 days
    assert _result['balance'] == tt.Asset.Test(196)
    assert _result['hbd_balance'] == tt.Asset.Tbd(1.404)

    response = wallet.api.get_conversion_requests('alice')

    assert response[0]['amount'] == tt.Asset.Tbd(0.5)

def is_valid_asset(asset_value):
    bool_value = bool(re.search(r'\d+\.\d{3}\sTESTS', asset_value))
    return bool_value
