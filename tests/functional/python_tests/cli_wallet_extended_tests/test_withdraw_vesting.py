from test_tools import Account, logger, World

def test_getters():
    with World() as world:
        init_node = world.create_init_node()
        init_node.run()

        wallet = init_node.attach_wallet()

        def check_withdraw_data( node, vesting_withdraw_rate, to_withdraw ):
          assert 'vesting_withdraw_rate' in node
          assert node['vesting_withdraw_rate'] == vesting_withdraw_rate

          assert 'to_withdraw' in node
          assert node['to_withdraw'] == to_withdraw

        def check_route_data( node ):
          assert 'operations' in node
          _ops = node['operations']

          assert len(_ops) == 1
          _op = _ops[0]

          assert 'type' in _op
          _op['type'] == 'set_withdraw_vesting_route_operation'

          assert 'value' in _op
          return _op['value']

        def check_route( node, from_account, to_account, percent, auto_vest ):
          assert 'from_account' in node
          assert node['from_account'] == from_account

          assert 'to_account' in node
          assert node['to_account'] == to_account

          assert 'percent' in node
          assert node['percent'] == percent

          assert 'auto_vest' in node
          assert node['auto_vest'] == auto_vest

        #**************************************************************
        logger.info('create_account...')
        response = wallet.api.create_account('initminer', 'alice', '{}')
        logger.info(response)

        assert 'result' in response
        _result = response['result']

        #**************************************************************
        logger.info('create_account...')
        response = wallet.api.create_account('initminer', 'bob', '{}')
        logger.info(response)

        assert 'result' in response
        _result = response['result']

        #**************************************************************
        logger.info('create_account...')
        response = wallet.api.create_account('initminer', 'carol', '{}')
        logger.info(response)

        assert 'result' in response
        _result = response['result']

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

        check_withdraw_data( _result, '0.000000 VESTS', 0 )

        #**************************************************************
        logger.info('get_withdraw_routes...')
        response = wallet.api.get_withdraw_routes('alice', 'by_destination')
        logger.info(response)

        assert 'result' in response
        _result = response['result']

        assert len(_result) == 0
        #**************************************************************
        logger.info('withdraw_vesting...')
        response = wallet.api.withdraw_vesting('alice', '4.000000 VESTS')
        logger.info(response)

        assert 'result' in response
        _result = response['result']

        #**************************************************************
        logger.info('get_account...')
        response = wallet.api.get_account('alice')
        logger.info(response)

        assert 'result' in response
        _result = response['result']

        check_withdraw_data( _result, '0.307693 VESTS', 4000000 )

        #**************************************************************
        logger.info('get_withdraw_routes...')
        response = wallet.api.get_withdraw_routes('alice', 'by_destination')
        logger.info(response)

        assert 'result' in response
        _result = response['result']

        assert len(_result) == 0

        #**************************************************************
        logger.info('set_withdraw_vesting_route...')
        response = wallet.api.set_withdraw_vesting_route('alice', 'bob', 30, True )
        logger.info(response)

        assert 'result' in response
        _result = response['result']

        _value = check_route_data( _result )
        check_route( _value, 'alice', 'bob', 30, True )

        #**************************************************************
        logger.info('get_withdraw_routes...')
        response = wallet.api.get_withdraw_routes('alice', 'by_withdraw_route')
        logger.info(response)

        assert 'result' in response
        _result = response['result']

        assert len(_result) == 1

        check_route( _result[0], 'alice', 'bob', 30, True )

        #**************************************************************
        logger.info('set_withdraw_vesting_route...')
        response = wallet.api.set_withdraw_vesting_route('alice', 'carol', 25, False )
        logger.info(response)

        assert 'result' in response
        _result = response['result']

        _value = check_route_data( _result )
        check_route( _value, 'alice', 'carol', 25, False )

        #**************************************************************
        logger.info('get_withdraw_routes...')
        response = wallet.api.get_withdraw_routes('alice', 'by_withdraw_route')
        logger.info(response)

        assert 'result' in response
        _result = response['result']

        assert len(_result) == 2

        check_route( _result[0], 'alice', 'bob', 30, True )
        check_route( _result[1], 'alice', 'carol', 25, False )
