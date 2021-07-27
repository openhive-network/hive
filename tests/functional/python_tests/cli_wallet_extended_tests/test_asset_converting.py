from test_tools import Account, logger, World, Asset

def test_conversion(world):
    init_node = world.create_init_node()
    init_node.run()

    wallet = init_node.attach_wallet()

    #**************************************************************
    response = wallet.api.create_account('initminer', 'alice', '{}')

    #**************************************************************
    response = wallet.api.transfer('initminer', 'alice', Asset.Test(200), 'avocado')

    #**************************************************************
    wallet.api.transfer_to_vesting('initminer', 'alice', Asset.Test(500))

    #**************************************************************
    response = wallet.api.get_account('alice')

    _result = response['result']

    assert _result['balance'] == Asset.Test(200)
    assert _result['hbd_balance'] == Asset.Tbd(0)

    #**************************************************************
    response = wallet.api.convert_hive_with_collateral('alice', Asset.Test(4))

    _result = response['result']

    _ops = _result['operations']
    _op = _ops[0]

    _value = _op[1]
    assert _value['amount'] == Asset.Test(4)

    #**************************************************************
    response = wallet.api.get_account('alice')

    _result = response['result']

    assert _result['balance'] == Asset.Test(196)
    assert _result['hbd_balance'] == '1.904 TBD'

    #**************************************************************
    response = wallet.api.get_collateralized_conversion_requests('alice')

    _result = response['result']

    _request = _result[0]

    assert _request['collateral_amount'] == Asset.Test(4)
    assert _request['converted_amount'] == '1.904 TBD'

    #**************************************************************
    response = wallet.api.estimate_hive_collateral(Asset.Tbd(4))

    _result = response['result']
    assert _result == '8.400 TESTS'

    #**************************************************************
    response = wallet.api.convert_hbd('alice', '0.500 TBD')

    _result = response['result']

    #**************************************************************
    response = wallet.api.get_account('alice')

    _result = response['result']

    #'balance' is still the same, because request of conversion will be done after 3.5 days
    assert _result['balance'] == Asset.Test(196)
    assert _result['hbd_balance'] == '1.404 TBD'

    #**************************************************************
    response = wallet.api.get_conversion_requests('alice')

    _result = response['result']

    __result = _result[0]

    assert __result['amount'] == '0.500 TBD'
