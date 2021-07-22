from test_tools import Account, logger, World

def test_withdraw_vesting(world):
    init_node = world.create_init_node()
    init_node.run()

    wallet = init_node.attach_wallet()

    def check_withdraw_data( node, vesting_withdraw_rate, to_withdraw ):
      assert node['vesting_withdraw_rate'] == vesting_withdraw_rate
      assert node['to_withdraw'] == to_withdraw

    def check_route_data( node ):
      _ops = node['operations']
      _op = _ops[0]

      _op[0] == 'set_withdraw_vesting_route'

      return _op[1]

    def check_route( node, from_account, to_account, percent, auto_vest ):
      assert node['from_account'] == from_account
      assert node['to_account'] == to_account
      assert node['percent'] == percent
      assert node['auto_vest'] == auto_vest

    #**************************************************************
    response = wallet.api.create_account('initminer', 'alice', '{}')
    assert 'result' in response

    #**************************************************************
    response = wallet.api.create_account('initminer', 'bob', '{}')
    assert 'result' in response

    #**************************************************************
    response = wallet.api.create_account('initminer', 'carol', '{}')
    assert 'result' in response

    #**************************************************************
    response = wallet.api.transfer_to_vesting('initminer', 'alice', '500.000 TESTS')
    assert 'result' in response

    #**************************************************************
    response = wallet.api.get_account('alice')
    _result = response['result']

    check_withdraw_data( _result, '0.000000 VESTS', 0 )

    #**************************************************************
    response = wallet.api.get_withdraw_routes('alice', 'by_destination')
    _result = response['result']

    assert len(_result) == 0
    #**************************************************************
    response = wallet.api.withdraw_vesting('alice', '4.000000 VESTS')
    _result = response['result']

    #**************************************************************
    response = wallet.api.get_account('alice')
    _result = response['result']

    check_withdraw_data( _result, '0.307693 VESTS', 4000000 )

    #**************************************************************
    response = wallet.api.get_withdraw_routes('alice', 'by_destination')
    _result = response['result']

    assert len(_result) == 0

    #**************************************************************
    response = wallet.api.set_withdraw_vesting_route('alice', 'bob', 30, True )
    _result = response['result']

    _value = check_route_data( _result )
    check_route( _value, 'alice', 'bob', 30, True )

    #**************************************************************
    response = wallet.api.get_withdraw_routes('alice', 'by_withdraw_route')
    _result = response['result']

    assert len(_result) == 1

    check_route( _result[0], 'alice', 'bob', 30, True )

    #**************************************************************
    response = wallet.api.set_withdraw_vesting_route('alice', 'carol', 25, False )
    _result = response['result']

    _value = check_route_data( _result )
    check_route( _value, 'alice', 'carol', 25, False )

    #**************************************************************
    response = wallet.api.get_withdraw_routes('alice', 'by_withdraw_route')
    _result = response['result']

    assert len(_result) == 2

    check_route( _result[0], 'alice', 'bob', 30, True )
    check_route( _result[1], 'alice', 'carol', 25, False )
