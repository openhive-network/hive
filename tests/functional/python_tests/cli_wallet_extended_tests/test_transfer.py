from test_tools import Account, logger, World

def test_transfer():
    with World() as world:
        init_node = world.create_init_node()
        init_node.run()

        wallet = init_node.attach_wallet()

        #**************************************************************
        logger.info('create_account...')
        response = wallet.api.create_account('initminer', 'newaccount', '{}')
        logger.info(response)

        #**************************************************************
        logger.info('create_account...')
        response = wallet.api.create_account('initminer', 'newaccount2', '{}')
        logger.info(response)

        #**************************************************************
        logger.info('transfer_to_vesting...')
        response = wallet.api.transfer_to_vesting('initminer', 'newaccount', '100.000 TESTS')
        logger.info(response)

        #**************************************************************
        logger.info('get_account...')
        response = wallet.api.get_account('newaccount')
        logger.info(response)

        _result = response['result']

        assert 'balance' in _result and _result['balance'] == '0.000 TESTS'
        assert 'hbd_balance' in _result and _result['hbd_balance'] == '0.000 TBD'
        assert 'savings_balance' in _result and _result['savings_balance'] == '0.000 TESTS'
        assert 'savings_hbd_balance' in _result and _result['savings_hbd_balance'] == '0.000 TBD'
        assert 'vesting_shares' in _result and _result['vesting_shares'] == '9.477704 VESTS'

        #**************************************************************
        logger.info('transfer...')
        response = wallet.api.transfer('initminer', 'newaccount', '5.432 TESTS', 'banana')
        logger.info(response)

        #**************************************************************
        logger.info('transfer...')
        response = wallet.api.transfer('initminer', 'newaccount', '9.169 TBD', 'banana2')
        logger.info(response)

        #**************************************************************
        logger.info('transfer_to_savings...')
        response = wallet.api.transfer_to_savings('initminer', 'newaccount', '15.432 TESTS', 'pomelo')
        logger.info(response)

        #**************************************************************
        logger.info('transfer_to_savings...')
        response = wallet.api.transfer_to_savings('initminer', 'newaccount', '19.169 TBD', 'pomelo2')
        logger.info(response)

        #**************************************************************
        logger.info('get_account...')
        response = wallet.api.get_account('newaccount')
        logger.info(response)

        _result = response['result']

        assert 'balance' in _result and _result['balance'] == '5.432 TESTS'
        assert 'hbd_balance' in _result and _result['hbd_balance'] == '9.169 TBD'
        assert 'savings_balance' in _result and _result['savings_balance'] == '15.432 TESTS'
        assert 'savings_hbd_balance' in _result and _result['savings_hbd_balance'] == '19.169 TBD'

        #**************************************************************
        logger.info('transfer_from_savings...')
        response = wallet.api.transfer_from_savings('newaccount', 7, 'newaccount2', '0.001 TESTS', 'kiwi')
        logger.info(response)

        #**************************************************************
        logger.info('transfer_from_savings...')
        response = wallet.api.transfer_from_savings('newaccount', 8, 'newaccount2', '0.001 TBD', 'kiwi2')
        logger.info(response)

        #**************************************************************
        logger.info('get_account...')
        response = wallet.api.get_account('newaccount')
        logger.info(response)

        _result = response['result']

        assert 'balance' in _result and _result['balance'] == '5.432 TESTS'
        assert 'hbd_balance' in _result and _result['hbd_balance'] == '9.169 TBD'
        assert 'savings_balance' in _result and _result['savings_balance'] == '15.431 TESTS'
        assert 'savings_hbd_balance' in _result and _result['savings_hbd_balance'] == '19.168 TBD'

        #**************************************************************
        logger.info('transfer_nonblocking...')
        response = wallet.api.transfer_nonblocking('newaccount', 'newaccount2', '0.100 TESTS', 'mango')
        logger.info(response)

        #**************************************************************
        logger.info('transfer_nonblocking...')
        response = wallet.api.transfer_nonblocking('newaccount', 'newaccount2', '0.200 TBD', 'mango2')
        logger.info(response)

        #**************************************************************
        logger.info('transfer_to_vesting_nonblocking...')
        response = wallet.api.transfer_to_vesting_nonblocking('initminer', 'newaccount', '100.000 TESTS')
        logger.info(response)

        logger.info('Waiting...')
        init_node.wait_number_of_blocks(1)

        #**************************************************************
        logger.info('get_account...')
        response = wallet.api.get_account('newaccount')
        logger.info(response)

        _result = response['result']

        assert 'balance' in _result and _result['balance'] == '5.332 TESTS'
        assert 'hbd_balance' in _result and _result['hbd_balance'] == '8.969 TBD'
        assert 'savings_balance' in _result and _result['savings_balance'] == '15.431 TESTS'
        assert 'vesting_shares' in _result and _result['vesting_shares'] == '18.739279 VESTS'

        #**************************************************************
        logger.info('cancel_transfer_from_savings...')
        response = wallet.api.cancel_transfer_from_savings('newaccount', 7)
        logger.info(response)

        #**************************************************************
        logger.info('get_account...')
        response = wallet.api.get_account('newaccount')
        logger.info(response)

        _result = response['result']

        assert 'savings_balance' in _result and _result['savings_balance'] == '15.432 TESTS'