from test_tools import Account, logger, World, Asset

def test_conversion(wallet):
    response = wallet.api.create_account('initminer', 'alice', '{}')

    response = wallet.api.transfer('initminer', 'alice', Asset.Test(200), 'avocado')

    wallet.api.transfer_to_vesting('initminer', 'alice', Asset.Test(500))

    response = wallet.api.get_account('alice')

    assert response['result']['balance'] == Asset.Test(200)
    assert response['result']['hbd_balance'] == Asset.Tbd(0)

    response = wallet.api.convert_hive_with_collateral('alice', Asset.Test(4))

    _ops = response['result']['operations']
    assert _ops[0][1]['amount'] == Asset.Test(4)

    response = wallet.api.get_account('alice')

    assert response['result']['balance'] == Asset.Test(196)
    assert response['result']['hbd_balance'] == Asset.Tbd(1.904)

    response = wallet.api.get_collateralized_conversion_requests('alice')

    _request = response['result'][0]
    assert _request['collateral_amount'] == Asset.Test(4)
    assert _request['converted_amount'] == Asset.Tbd(1.904)

    response = wallet.api.estimate_hive_collateral(Asset.Tbd(4))

    assert response['result'] == Asset.Test(8.4)

    wallet.api.convert_hbd('alice', Asset.Tbd(0.5))

    response = wallet.api.get_account('alice')

    _result = response['result']
    #'balance' is still the same, because request of conversion will be done after 3.5 days
    assert _result['balance'] == Asset.Test(196)
    assert _result['hbd_balance'] == Asset.Tbd(1.404)

    response = wallet.api.get_conversion_requests('alice')

    assert response['result'][0]['amount'] == Asset.Tbd(0.5)
