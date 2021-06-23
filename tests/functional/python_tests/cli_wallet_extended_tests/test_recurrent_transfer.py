from test_tools import Account, logger, World

def test_transfer():
    with World() as world:
        init_node = world.create_init_node()
        init_node.run()

        wallet = init_node.attach_wallet()

        def check_recurrence_transfer_data( node ):
          assert 'operations' in node
          _ops = node['operations']

          assert len(_ops) == 1
          _op = _ops[0]

          assert 'type' in _op
          assert _op['type'] == 'recurrent_transfer_operation'

          assert 'value' in _op
          return _op['value']

        def check_recurrence_transfer( node, _from, to, amount, memo, recurrence, executions_key, executions_number ):
          assert 'from' in node and node['from'] == _from
          assert 'to' in node and node['to'] == to
          assert 'amount' in node and node['amount'] == amount
          assert 'memo' in node and node['memo'] == memo
          assert 'recurrence' in node and node['recurrence'] == recurrence
          assert executions_key in node and node[executions_key] == executions_number

        #**************************************************************
        logger.info('create_account...')
        response = wallet.api.create_account('initminer', 'alice', '{}')
        logger.info(response)

        assert 'result' in response

        #**************************************************************
        logger.info('create_account...')
        response = wallet.api.create_account('initminer', 'bob', '{}')
        logger.info(response)

        assert 'result' in response

        #**************************************************************
        logger.info('transfer_to_vesting...')
        response = wallet.api.transfer_to_vesting('initminer', 'alice', '100.000 TESTS')
        logger.info(response)

        assert 'result' in response

        #**************************************************************
        logger.info('transfer_to_vesting...')
        response = wallet.api.transfer_to_vesting('initminer', 'bob', '100.000 TESTS')
        logger.info(response)

        assert 'result' in response

        #**************************************************************
        logger.info('transfer...')
        response = wallet.api.transfer('initminer', 'alice', '500.000 TESTS', 'banana')
        logger.info(response)

        assert 'result' in response

        #**************************************************************
        logger.info('recurrent_transfer...')
        response = wallet.api.recurrent_transfer('alice', 'bob', '20.000 TESTS', 'banana-cherry', 24, 3 )
        logger.info(response)

        _result = response['result']

        _value = check_recurrence_transfer_data( _result )
  
        check_recurrence_transfer( _value, 'alice', 'bob', '20.000 TESTS', 'banana-cherry', 24, 'executions', 3 )

        #**************************************************************
        logger.info('find_recurrent_transfers...')
        response = wallet.api.find_recurrent_transfers('alice')
        logger.info(response)

        _result = response['result']

        assert len(_result) == 1

        #Here is '2' because one recurrent transfer was done in the same block as block number where `recurrent_transfer` was executed
        check_recurrence_transfer( _result[0], 'alice', 'bob', '20.000 TESTS', 'banana-cherry', 24, 'remaining_executions', 2 )

        #**************************************************************
        logger.info('recurrent_transfer...')
        response = wallet.api.recurrent_transfer('bob', 'alice', '0.900 TESTS', 'banana-lime', 25, 2 )
        logger.info(response)

        _result = response['result']

        _value = check_recurrence_transfer_data( _result )
  
        check_recurrence_transfer( _value, 'bob', 'alice', '0.900 TESTS', 'banana-lime', 25, 'executions', 2 )

        #**************************************************************
        logger.info('find_recurrent_transfers...')
        response = wallet.api.find_recurrent_transfers('bob')
        logger.info(response)

        _result = response['result']

        assert len(_result) == 1

        #Here is '1' because one recurrent transfer was done in the same block as block number where `recurrent_transfer` was executed
        check_recurrence_transfer( _result[0], 'bob', 'alice', '0.900 TESTS', 'banana-lime', 25, 'remaining_executions', 1 )

        #**************************************************************
        logger.info('recurrent_transfer...')
        response = wallet.api.recurrent_transfer('bob', 'initminer', '0.800 TESTS', 'banana-lemon', 26, 22 )
        logger.info(response)

        _result = response['result']

        _value = check_recurrence_transfer_data( _result )
  
        check_recurrence_transfer( _value, 'bob', 'initminer', '0.800 TESTS', 'banana-lemon', 26, 'executions', 22 )

        #**************************************************************
        logger.info('find_recurrent_transfers...')
        response = wallet.api.find_recurrent_transfers('bob')
        logger.info(response)

        _result = response['result']

        assert len(_result) == 2

        check_recurrence_transfer( _result[0], 'bob', 'alice', '0.900 TESTS', 'banana-lime', 25, 'remaining_executions', 1 )
        check_recurrence_transfer( _result[1], 'bob', 'initminer', '0.800 TESTS', 'banana-lemon', 26, 'remaining_executions', 21 )