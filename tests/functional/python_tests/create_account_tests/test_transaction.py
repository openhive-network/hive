from test_tools import Account, logger, World
import dateutil.parser as dp
import datetime

def test_transfer():
    with World() as world:
        init_node = world.create_init_node()
        init_node.config.plugin.append('database_api')
        init_node.config.plugin.append('network_broadcast_api')
        init_node.run()

        wallet = init_node.attach_wallet()

        #**************************************************************
        logger.info('create_account...')
        response = wallet.api.create_account('initminer', 'carol', '{}')
        logger.info(response)

        #**************************************************************
        logger.info('transfer_to_vesting/transfer/transfer...')
        with wallet.in_single_transaction(broadcast=False) as transaction:
            wallet.api.transfer_to_vesting('initminer', 'carol', '100.000 TESTS')
            wallet.api.transfer('initminer', 'carol', '500.000 TESTS', 'kiwi')
            wallet.api.transfer('initminer', 'carol', '50.000 TBD', 'orange')

        trx_response = transaction.get_response()
        logger.info(trx_response)

        assert 'result' in trx_response
        _result_trx_response = trx_response['result']

        #**************************************************************
        logger.info('get_account...')
        response = wallet.api.get_account('carol')
        logger.info(response)

        assert 'result' in response
        _result = response['result']

        assert 'balance' in _result and _result['balance'] == '0.000 TESTS'
        assert 'hbd_balance' in _result and _result['hbd_balance'] == '0.000 TBD'
        assert 'vesting_shares' in _result and _result['vesting_shares'] == '0.000000 VESTS'

        #**************************************************************
        logger.info('serialize_transaction...')
        response = wallet.api.serialize_transaction(_result_trx_response)
        logger.info(response)

        assert 'result' in response and response['result'] != '00000000000000000000000000'

        #**************************************************************
        logger.info('sign_transaction...')
        response = wallet.api.sign_transaction(_result_trx_response)
        logger.info(response)

        #**************************************************************
        logger.info('get_account...')
        response = wallet.api.get_account('carol')
        logger.info(response)

        assert 'result' in response
        _result = response['result']

        assert 'balance' in _result and _result['balance'] == '500.000 TESTS'
        assert 'hbd_balance' in _result and _result['hbd_balance'] == '50.000 TBD'
        assert 'vesting_shares' in _result and _result['vesting_shares'] != '0.000000 VESTS'

        #**************************************************************
        _time = datetime.datetime.utcnow()
        _before_seconds = (int)(_time.timestamp())
        logger.info('_time: {} seconds:{}...'.format(_time, _before_seconds))

        logger.info('transfer_to_savings...')
        response = wallet.api.transfer_to_savings('initminer', 'carol', '0.007 TESTS', 'plum')
        logger.info(response)

        assert 'result' in response and 'expiration' in response['result']
        _expiration = response['result']['expiration']

        parsed_t = dp.parse(_expiration)
        t_in_seconds = parsed_t.timestamp()
        logger.info('_time: {} seconds:{}...'.format(_expiration, t_in_seconds))

        _val = t_in_seconds - _before_seconds
        assert _val == 30 or _val == 31
        #**************************************************************
        logger.info('set_transaction_expiration...')
        response = wallet.api.set_transaction_expiration(678)
        logger.info(response)

        assert 'result' in response
        response['result'] = None

        #**************************************************************
        _time = datetime.datetime.utcnow()
        _before_seconds = (int)(_time.timestamp())
        logger.info('_time: {} seconds:{}...'.format(_time, _before_seconds))

        logger.info('transfer_to_savings...')
        response = wallet.api.transfer_to_savings('initminer', 'carol', '0.008 TESTS', 'lemon')
        logger.info(response)

        assert 'result' in response and 'expiration' in response['result']
        _expiration = response['result']['expiration']

        parsed_t = dp.parse(_expiration)
        t_in_seconds = parsed_t.timestamp()
        logger.info('_time: {} seconds:{}...'.format(_expiration, t_in_seconds))

        _val = t_in_seconds - _before_seconds
        assert _val == 678 or _val == 679
