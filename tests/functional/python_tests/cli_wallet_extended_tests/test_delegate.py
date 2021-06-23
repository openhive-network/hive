from test_tools import Account, logger, World

def test_delegate():
    with World() as world:
        init_node = world.create_init_node()
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
        logger.info('transfer...')
        response = wallet.api.transfer('initminer', 'alice', '100.000 TBD', 'banana')
        logger.info(response)

        #**************************************************************
        logger.info('transfer_to_vesting...')
        response = wallet.api.transfer_to_vesting('initminer', 'alice', '50.000 TESTS')
        logger.info(response)

        #**************************************************************
        logger.info('create_account...')
        response = wallet.api.create_account('alice', 'bob', '{}')
        logger.info(response)

        #**************************************************************
        logger.info('transfer...')
        response = wallet.api.transfer('alice', 'bob', '50.000 TESTS', 'lemon')
        logger.info(response)

        #**************************************************************
        logger.info('transfer_to_vesting...')
        response = wallet.api.transfer_to_vesting('alice', 'bob', '25.000 TESTS')
        logger.info(response)

        #**************************************************************
        logger.info('delegate_vesting_shares...')
        response = wallet.api.delegate_vesting_shares('alice', 'bob', '1.123456 VESTS')
        logger.info(response)

        #**************************************************************
        logger.info('get_account...')
        response = wallet.api.get_account('alice')
        logger.info(response)

        _result = response['result']

        assert 'delegated_vesting_shares' in _result and _result['delegated_vesting_shares'] == '1.123456 VESTS'
        assert 'balance' in _result and _result['balance'] == '125.000 TESTS'
        assert 'hbd_balance' in _result and _result['hbd_balance'] == '100.000 TBD'

        #**************************************************************
        logger.info('get_account...')
        response = wallet.api.get_account('bob')
        logger.info(response)

        _result = response['result']

        assert 'received_vesting_shares' in _result and _result['received_vesting_shares'] == '1.123456 VESTS'
        assert 'balance' in _result and _result['balance'] == '50.000 TESTS'
        assert 'hbd_balance' in _result and _result['hbd_balance'] == '0.000 TBD'

        #**************************************************************
        logger.info('delegate_vesting_shares_and_transfer...')
        response = wallet.api.delegate_vesting_shares_and_transfer('alice', 'bob', '1.000000 VESTS', '6.666 TBD', 'watermelon')
        logger.info(response)

        #**************************************************************
        logger.info('get_account...')
        response = wallet.api.get_account('alice')
        logger.info(response)

        _result = response['result']

        assert 'delegated_vesting_shares' in _result and _result['delegated_vesting_shares'] == '1.123456 VESTS'
        assert 'balance' in _result and _result['balance'] == '125.000 TESTS'
        assert 'hbd_balance' in _result and _result['hbd_balance'] == '93.334 TBD'

        #**************************************************************
        logger.info('get_account...')
        response = wallet.api.get_account('bob')
        logger.info(response)

        _result = response['result']

        assert 'received_vesting_shares' in _result and _result['received_vesting_shares'] == '1.000000 VESTS'
        assert 'balance' in _result and _result['balance'] == '50.000 TESTS'
        assert 'hbd_balance' in _result and _result['hbd_balance'] == '6.666 TBD'

        #**************************************************************
        logger.info('delegate_vesting_shares_nonblocking...')
        response = wallet.api.delegate_vesting_shares_nonblocking('bob', 'alice', '0.100000 VESTS')
        logger.info(response)

        logger.info('Waiting...')
        init_node.wait_number_of_blocks(1)

        #**************************************************************
        logger.info('get_account...')
        response = wallet.api.get_account('alice')
        logger.info(response)

        _result = response['result']

        assert 'delegated_vesting_shares' in _result and _result['delegated_vesting_shares'] == '1.123456 VESTS'
        assert 'received_vesting_shares' in _result and _result['received_vesting_shares'] == '0.100000 VESTS'
        assert 'balance' in _result and _result['balance'] == '125.000 TESTS'
        assert 'hbd_balance' in _result and _result['hbd_balance'] == '93.334 TBD'

        #**************************************************************
        logger.info('get_account...')
        response = wallet.api.get_account('bob')
        logger.info(response)

        _result = response['result']

        assert 'delegated_vesting_shares' in _result and _result['delegated_vesting_shares'] == '0.100000 VESTS'
        assert 'received_vesting_shares' in _result and _result['received_vesting_shares'] == '1.000000 VESTS'
        assert 'balance' in _result and _result['balance'] == '50.000 TESTS'
        assert 'hbd_balance' in _result and _result['hbd_balance'] == '6.666 TBD'

        #**************************************************************
        logger.info('delegate_vesting_shares_and_transfer_nonblocking...')
        response = wallet.api.delegate_vesting_shares_and_transfer_nonblocking('bob', 'alice', '0.100000 VESTS', '6.555 TBD', 'pear')
        logger.info(response)

        logger.info('Waiting...')
        init_node.wait_number_of_blocks(1)

        #**************************************************************
        logger.info('get_account...')
        response = wallet.api.get_account('alice')
        logger.info(response)

        _result = response['result']

        assert 'delegated_vesting_shares' in _result and _result['delegated_vesting_shares'] == '1.123456 VESTS'
        assert 'received_vesting_shares' in _result and _result['received_vesting_shares'] == '0.100000 VESTS'
        assert 'balance' in _result and _result['balance'] == '125.000 TESTS'
        assert 'hbd_balance' in _result and _result['hbd_balance'] == '99.889 TBD'

        #**************************************************************
        logger.info('get_account...')
        response = wallet.api.get_account('bob')
        logger.info(response)

        _result = response['result']

        assert 'delegated_vesting_shares' in _result and _result['delegated_vesting_shares'] == '0.100000 VESTS'
        assert 'received_vesting_shares' in _result and _result['received_vesting_shares'] == '1.000000 VESTS'
        assert 'balance' in _result and _result['balance'] == '50.000 TESTS'
        assert 'hbd_balance' in _result and _result['hbd_balance'] == '0.111 TBD'
