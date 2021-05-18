from test_tools import Account, logger, World

def test_getters():
    with World() as world:
        init_node = world.create_init_node()
        init_node.config.plugin.append('database_api')
        init_node.config.plugin.append('network_broadcast_api')
        init_node.config.plugin.append('account_history_rocksdb')
        init_node.config.plugin.append('account_history')
        init_node.config.plugin.append('account_history_api')
        init_node.config.plugin.append('block_api')
        init_node.run()

        wallet = init_node.attach_wallet()

        #**************************************************************
        logger.info('create_account...')
        response = wallet.api.create_account('initminer', 'alice', '{}')
        logger.info(response)

        #**************************************************************
        logger.info('transfer...')
        response = wallet.api.transfer('initminer', 'alice', '200.000 TESTS', 'avocado')
        logger.info(response)

        #**************************************************************
        logger.info('transfer_to_vesting...')
        response = wallet.api.transfer_to_vesting('initminer', 'alice', '500.000 TESTS')
        logger.info(response)

        assert 'result' in response
        _result = response['result']

        assert 'ref_block_num' in _result
        block_number = _result['ref_block_num'] + 1

        #**************************************************************
        logger.info('get_account...')
        response = wallet.api.get_account('alice')
        logger.info(response)

        assert 'result' in response
        _result = response['result']

        assert 'balance' in _result and _result['balance'] == '200.000 TESTS'
        assert 'hbd_balance' in _result and _result['hbd_balance'] == '0.000 TBD'

        #**************************************************************
        logger.info('convert_hive_with_collateral...')
        response = wallet.api.convert_hive_with_collateral('alice', '4.000 TESTS')
        logger.info(response)

        assert 'result' in response
        _result = response['result']

        assert 'operations' in _result
        _ops = _result['operations']

        assert len(_ops) == 1
        _op = _ops[0]

        assert 'value' in _op
        _value = _op['value']

        assert 'amount' in _value
        assert _value['amount'] == '4.000 TESTS'

        #**************************************************************
        logger.info('get_account...')
        response = wallet.api.get_account('alice')
        logger.info(response)

        assert 'result' in response
        _result = response['result']

        assert 'balance' in _result and _result['balance'] == '196.000 TESTS'
        assert 'hbd_balance' in _result and _result['hbd_balance'] == '1.904 TBD'
        #**************************************************************
        logger.info('get_collateralized_conversion_requests...')
        response = wallet.api.get_collateralized_conversion_requests('alice')
        logger.info(response)

        assert 'result' in response
        _result = response['result']

        assert len(_result) == 1
        _request = _result[0]

        assert 'collateral_amount' in _request
        assert _request['collateral_amount'] == '4.000 TESTS'

        assert 'converted_amount' in _request
        assert _request['converted_amount'] == '1.904 TBD'

        #**************************************************************
        logger.info('estimate_hive_collateral...')
        response = wallet.api.estimate_hive_collateral('4.000 TBD')
        logger.info(response)

        assert 'result' in response
        _result = response['result']

        assert _result == '8.400 TESTS'

        #**************************************************************
        logger.info('convert_hbd...')
        response = wallet.api.convert_hbd('alice', '0.500 TBD')
        logger.info(response)

        assert 'result' in response
        _result = response['result']

        #**************************************************************
        logger.info('get_account...')
        response = wallet.api.get_account('alice')
        logger.info(response)

        assert 'result' in response
        _result = response['result']

        #'balance' is still the same, because request of conversion will be done after 3.5 days
        assert 'balance' in _result and _result['balance'] == '196.000 TESTS'
        assert 'hbd_balance' in _result and _result['hbd_balance'] == '1.404 TBD'

        #**************************************************************
        logger.info('get_conversion_requests...')
        response = wallet.api.get_conversion_requests('alice')
        logger.info(response)

        assert 'result' in response
        _result = response['result']

        assert len(_result) == 1
        __result = _result[0]

        assert 'amount' in __result
        assert __result['amount'] == '0.500 TBD'
