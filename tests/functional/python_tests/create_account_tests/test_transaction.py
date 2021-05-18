from test_tools import Account, logger, World

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

        _response = transaction.get_response()
        logger.info(_response)

        #**************************************************************
        logger.info('get_account...')
        response = wallet.api.get_account('carol')
        logger.info(response)

        assert 'result' in response
        _result = response['result']

        assert 'balance' in _result and _result['balance'] == '0.000 TESTS'
        assert 'hbd_balance' in _result and _result['hbd_balance'] == '0.000 TBD'
        assert 'vesting_shares' in _result and _result['vesting_shares'] == '0.000000 VESTS'

        #'balance': '500.000 TESTS',
        #'hbd_balance': '50.000 TBD',
        #'vesting_shares': '21.973069 VESTS',