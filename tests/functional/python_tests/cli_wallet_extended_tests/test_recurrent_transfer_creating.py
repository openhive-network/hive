from test_tools import Account, logger, World

def test_recurrent_transfer():
    with World() as world:
        init_node = world.create_init_node()
        init_node.run()

        wallet = init_node.attach_wallet()

        def check_recurrence_transfer_data( node ):
          _ops = node['operations']

          _op = _ops[0]

          assert _op['type'] == 'recurrent_transfer_operation'

          return _op['value']

        def check_recurrence_transfer( node, _from, to, amount, memo, recurrence, executions_key, executions_number ):
          assert node['from'] == _from
          assert node['to'] == to
          assert node['amount'] == amount
          assert node['memo'] == memo
          assert node['recurrence'] == recurrence
          assert node[executions_key] == executions_number

        #**************************************************************
        response = wallet.api.create_account('initminer', 'alice', '{}')
        assert 'result' in response

        #**************************************************************
        response = wallet.api.create_account('initminer', 'bob', '{}')
        assert 'result' in response

        #**************************************************************
        response = wallet.api.transfer_to_vesting('initminer', 'alice', '100.000 TESTS')
        assert 'result' in response

        #**************************************************************
        response = wallet.api.transfer_to_vesting('initminer', 'bob', '100.000 TESTS')
        assert 'result' in response

        #**************************************************************
        response = wallet.api.transfer('initminer', 'alice', '500.000 TESTS', 'banana')
        assert 'result' in response

        #**************************************************************
        response = wallet.api.recurrent_transfer('alice', 'bob', '20.000 TESTS', 'banana-cherry', 24, 3 )
        _result = response['result']

        _value = check_recurrence_transfer_data( _result )
  
        check_recurrence_transfer( _value, 'alice', 'bob', '20.000 TESTS', 'banana-cherry', 24, 'executions', 3 )

        #**************************************************************
        response = wallet.api.find_recurrent_transfers('alice')
        _result = response['result']

        assert len(_result) == 1

        #Here is '2' because one recurrent transfer was done in the same block as block number where `recurrent_transfer` was executed
        check_recurrence_transfer( _result[0], 'alice', 'bob', '20.000 TESTS', 'banana-cherry', 24, 'remaining_executions', 2 )

        #**************************************************************
        response = wallet.api.recurrent_transfer('bob', 'alice', '0.900 TESTS', 'banana-lime', 25, 2 )
        _result = response['result']

        _value = check_recurrence_transfer_data( _result )
  
        check_recurrence_transfer( _value, 'bob', 'alice', '0.900 TESTS', 'banana-lime', 25, 'executions', 2 )

        #**************************************************************
        response = wallet.api.find_recurrent_transfers('bob')
        _result = response['result']

        assert len(_result) == 1

        #Here is '1' because one recurrent transfer was done in the same block as block number where `recurrent_transfer` was executed
        check_recurrence_transfer( _result[0], 'bob', 'alice', '0.900 TESTS', 'banana-lime', 25, 'remaining_executions', 1 )

        #**************************************************************
        response = wallet.api.recurrent_transfer('bob', 'initminer', '0.800 TESTS', 'banana-lemon', 26, 22 )
        _result = response['result']

        _value = check_recurrence_transfer_data( _result )
  
        check_recurrence_transfer( _value, 'bob', 'initminer', '0.800 TESTS', 'banana-lemon', 26, 'executions', 22 )

        #**************************************************************
        response = wallet.api.find_recurrent_transfers('bob')
        _result = response['result']

        assert len(_result) == 2

        check_recurrence_transfer( _result[0], 'bob', 'alice', '0.900 TESTS', 'banana-lime', 25, 'remaining_executions', 1 )
        check_recurrence_transfer( _result[1], 'bob', 'initminer', '0.800 TESTS', 'banana-lemon', 26, 'remaining_executions', 21 )