from test_tools import Account, logger, World

def test_escrow():
    with World() as world:
        init_node = world.create_init_node()
        init_node.config.plugin.append('database_api')
        init_node.config.plugin.append('network_broadcast_api')
        init_node.run()

        wallet = init_node.attach_wallet()

        logger.info('Waiting until initminer will be able to create account...')
        init_node.wait_number_of_blocks(3)

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
        logger.info('create_account...')
        response = wallet.api.create_account('alice', 'carol', '{}')
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
        logger.info('get_accounts...')
        response = wallet.api.get_accounts(['alice', 'bob', 'carol'])
        logger.info(response)

        assert 'result' in response
        _result = response['result']
        assert len(_result) == 3

        _alice = _result[0]
        assert 'balance' in _alice and _alice['balance'] == '125.000 TESTS'
        assert 'hbd_balance' in _alice and _alice['hbd_balance'] == '100.000 TBD'

        _bob = _result[1]
        assert 'balance' in _bob and _bob['balance'] == '50.000 TESTS'
        assert 'hbd_balance' in _bob and _bob['hbd_balance'] == '0.000 TBD'

        _carol = _result[2]
        assert 'balance' in _carol and _carol['balance'] == '0.000 TESTS'
        assert 'hbd_balance' in _carol and _carol['hbd_balance'] == '0.000 TBD'

        # #**************************************************************
        logger.info('escrow_transfer...')
        response = wallet.api.escrow_transfer('alice', 'bob', 'carol', 99, '1.000 TBD', '2.000 TESTS', '5.000 TBD', '2029-06-02T00:00:00', '2029-06-02T01:01:01', '{}')
        logger.info(response)

        #**************************************************************
        logger.info('get_accounts...')
        response = wallet.api.get_accounts(['alice', 'bob', 'carol'])
        logger.info(response)

        assert 'result' in response
        _result = response['result']
        assert len(_result) == 3

        _alice = _result[0]
        assert 'balance' in _alice and _alice['balance'] == '123.000 TESTS'
        assert 'hbd_balance' in _alice and _alice['hbd_balance'] == '94.000 TBD'

        _bob = _result[1]
        assert 'balance' in _bob and _bob['balance'] == '50.000 TESTS'
        assert 'hbd_balance' in _bob and _bob['hbd_balance'] == '0.000 TBD'

        _carol = _result[2]
        assert 'balance' in _carol and _carol['balance'] == '0.000 TESTS'
        assert 'hbd_balance' in _carol and _carol['hbd_balance'] == '0.000 TBD'

        #**************************************************************
        logger.info('transfer_to_vesting...')
        response = wallet.api.transfer_to_vesting('initminer', 'carol', '50.000 TESTS')
        logger.info(response)

        #**************************************************************
        logger.info('escrow_approve...')
        response = wallet.api.escrow_approve('alice', 'bob', 'carol', 'carol', 99, True)
        logger.info(response)

        #**************************************************************
        logger.info('get_accounts...')
        response = wallet.api.get_accounts(['alice', 'bob', 'carol'])
        logger.info(response)

        assert 'result' in response
        _result = response['result']
        assert len(_result) == 3

        _alice = _result[0]
        assert 'balance' in _alice and _alice['balance'] == '123.000 TESTS'
        assert 'hbd_balance' in _alice and _alice['hbd_balance'] == '94.000 TBD'

        _bob = _result[1]
        assert 'balance' in _bob and _bob['balance'] == '50.000 TESTS'
        assert 'hbd_balance' in _bob and _bob['hbd_balance'] == '0.000 TBD'

        _carol = _result[2]
        assert 'balance' in _carol and _carol['balance'] == '0.000 TESTS'
        assert 'hbd_balance' in _carol and _carol['hbd_balance'] == '0.000 TBD'

        #**************************************************************
        logger.info('escrow_approve...')
        response = wallet.api.escrow_approve('alice', 'bob', 'carol', 'bob', 99, True)
        logger.info(response)

        #**************************************************************
        logger.info('get_accounts...')
        response = wallet.api.get_accounts(['alice', 'bob', 'carol'])
        logger.info(response)

        assert 'result' in response
        _result = response['result']
        assert len(_result) == 3

        _alice = _result[0]
        assert 'balance' in _alice and _alice['balance'] == '123.000 TESTS'
        assert 'hbd_balance' in _alice and _alice['hbd_balance'] == '94.000 TBD'

        _bob = _result[1]
        assert 'balance' in _bob and _bob['balance'] == '50.000 TESTS'
        assert 'hbd_balance' in _bob and _bob['hbd_balance'] == '0.000 TBD'

        _carol = _result[2]
        assert 'balance' in _carol and _carol['balance'] == '0.000 TESTS'
        assert 'hbd_balance' in _carol and _carol['hbd_balance'] == '5.000 TBD'

        #**************************************************************
        logger.info('escrow_dispute...')
        response = wallet.api.escrow_dispute('alice', 'bob', 'carol', 'alice', 99)
        logger.info(response)

        #**************************************************************
        logger.info('get_accounts...')
        response = wallet.api.get_accounts(['alice', 'bob', 'carol'])
        logger.info(response)

        assert 'result' in response
        _result = response['result']
        assert len(_result) == 3

        _alice = _result[0]
        assert 'balance' in _alice and _alice['balance'] == '123.000 TESTS'
        assert 'hbd_balance' in _alice and _alice['hbd_balance'] == '94.000 TBD'

        _bob = _result[1]
        assert 'balance' in _bob and _bob['balance'] == '50.000 TESTS'
        assert 'hbd_balance' in _bob and _bob['hbd_balance'] == '0.000 TBD'

        _carol = _result[2]
        assert 'balance' in _carol and _carol['balance'] == '0.000 TESTS'
        assert 'hbd_balance' in _carol and _carol['hbd_balance'] == '5.000 TBD'

        #**************************************************************
        logger.info('escrow_release...')
        response = wallet.api.escrow_release('alice', 'bob', 'carol', 'carol', 'bob', 99, '1.000 TBD', '2.000 TESTS')
        logger.info(response)

        #**************************************************************
        logger.info('get_accounts...')
        response = wallet.api.get_accounts(['alice', 'bob', 'carol'])
        logger.info(response)

        assert 'result' in response
        _result = response['result']
        assert len(_result) == 3

        _alice = _result[0]
        assert 'balance' in _alice and _alice['balance'] == '123.000 TESTS'
        assert 'hbd_balance' in _alice and _alice['hbd_balance'] == '94.000 TBD'

        _bob = _result[1]
        assert 'balance' in _bob and _bob['balance'] == '52.000 TESTS'
        assert 'hbd_balance' in _bob and _bob['hbd_balance'] == '1.000 TBD'

        _carol = _result[2]
        assert 'balance' in _carol and _carol['balance'] == '0.000 TESTS'
        assert 'hbd_balance' in _carol and _carol['hbd_balance'] == '5.000 TBD'
