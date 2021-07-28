from test_tools import Account, logger, World, Asset

def test_recurrent_transfer(wallet):
    def check_recurrence_transfer_data( node ):
      _ops = node['operations']

      _op = _ops[0]

      assert _op[0] == 'recurrent_transfer'

      return _op[1]

    def check_recurrence_transfer( node, _from, to, amount, memo, recurrence, executions_key, executions_number ):
      assert node['from'] == _from
      assert node['to'] == to
      assert node['amount'] == amount
      assert node['memo'] == memo
      assert node['recurrence'] == recurrence
      assert node[executions_key] == executions_number

    #**************************************************************
    wallet.api.create_account('initminer', 'alice', '{}')

    #**************************************************************
    wallet.api.create_account('initminer', 'bob', '{}')

    #**************************************************************
    wallet.api.transfer_to_vesting('initminer', 'alice', Asset.Test(100))

    #**************************************************************
    wallet.api.transfer_to_vesting('initminer', 'bob', Asset.Test(100))

    #**************************************************************
    wallet.api.transfer('initminer', 'alice', Asset.Test(500), 'banana')

    #**************************************************************
    response = wallet.api.recurrent_transfer('alice', 'bob', Asset.Test(20), 'banana-cherry', 24, 3 )

    _value = check_recurrence_transfer_data( response['result'] )

    check_recurrence_transfer( _value, 'alice', 'bob', Asset.Test(20), 'banana-cherry', 24, 'executions', 3 )

    #**************************************************************
    response = wallet.api.find_recurrent_transfers('alice')

    _result = response['result']

    assert len(_result) == 1

    check_recurrence_transfer( _result[0], 'alice', 'bob', Asset.Test(20), 'banana-cherry', 24, 'remaining_executions', 2 )

    #**************************************************************
    response = wallet.api.recurrent_transfer('bob', 'alice', '0.900 TESTS', 'banana-lime', 25, 2 )

    _value = check_recurrence_transfer_data( response['result'] )

    check_recurrence_transfer( _value, 'bob', 'alice', '0.900 TESTS', 'banana-lime', 25, 'executions', 2 )

    #**************************************************************
    response = wallet.api.find_recurrent_transfers('bob')

    _result = response['result']

    assert len(_result) == 1

    check_recurrence_transfer( _result[0], 'bob', 'alice', '0.900 TESTS', 'banana-lime', 25, 'remaining_executions', 1 )

    #**************************************************************
    response = wallet.api.recurrent_transfer('bob', 'initminer', '0.800 TESTS', 'banana-lemon', 26, 22 )

    _value = check_recurrence_transfer_data( response['result'] )

    check_recurrence_transfer( _value, 'bob', 'initminer', '0.800 TESTS', 'banana-lemon', 26, 'executions', 22 )

    #**************************************************************
    response = wallet.api.find_recurrent_transfers('bob')

    _result = response['result']

    assert len(_result) == 2

    check_recurrence_transfer( _result[0], 'bob', 'alice', '0.900 TESTS', 'banana-lime', 25, 'remaining_executions', 1 )
    check_recurrence_transfer( _result[1], 'bob', 'initminer', '0.800 TESTS', 'banana-lemon', 26, 'remaining_executions', 21 )
