from test_tools import Account, logger, World

def test_conversion():
    with World() as world:
        init_node = world.create_init_node()
        init_node.run()

        wallet = init_node.attach_wallet()

        #**************************************************************
        response = wallet.api.create_account('initminer', 'alice', '{}')

        #**************************************************************
        response = wallet.api.transfer('initminer', 'alice', '200.000 TESTS', 'avocado')

        #**************************************************************
        response = wallet.api.transfer_to_vesting('initminer', 'alice', '500.000 TESTS')
        assert 'result' in response

        #**************************************************************
        response = wallet.api.get_account('alice')

        _result = response['result']

        assert _result['balance'] == '200.000 TESTS'
        assert _result['hbd_balance'] == '0.000 TBD'

        #**************************************************************
        response = wallet.api.convert_hive_with_collateral('alice', '4.000 TESTS')

        _result = response['result']

        _ops = _result['operations']
        _op = _ops[0]

        _value = _op[1]
        assert _value['amount'] == '4.000 TESTS'

        #**************************************************************
        response = wallet.api.get_account('alice')

        _result = response['result']

        assert _result['balance'] == '196.000 TESTS'
        assert _result['hbd_balance'] == '1.904 TBD'

        #**************************************************************
        response = wallet.api.get_collateralized_conversion_requests('alice')

        _result = response['result']

        _request = _result[0]

        assert _request['collateral_amount'] == '4.000 TESTS'
        assert _request['converted_amount'] == '1.904 TBD'

        #**************************************************************
        response = wallet.api.estimate_hive_collateral('4.000 TBD')

        _result = response['result']
        assert _result == '8.400 TESTS'

        #**************************************************************
        response = wallet.api.convert_hbd('alice', '0.500 TBD')

        _result = response['result']

        #**************************************************************
        response = wallet.api.get_account('alice')

        _result = response['result']

        #'balance' is still the same, because request of conversion will be done after 3.5 days
        assert _result['balance'] == '196.000 TESTS'
        assert _result['hbd_balance'] == '1.404 TBD'

        #**************************************************************
        response = wallet.api.get_conversion_requests('alice')

        _result = response['result']

        __result = _result[0]

        assert __result['amount'] == '0.500 TBD'
