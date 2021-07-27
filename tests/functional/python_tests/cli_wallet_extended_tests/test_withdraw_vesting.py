from test_tools import Account, logger, World, Asset

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
    wallet.api.create_account('initminer', 'alice', '{}')

    #**************************************************************
    wallet.api.create_account('initminer', 'bob', '{}')

    #**************************************************************
    wallet.api.create_account('initminer', 'carol', '{}')

    #**************************************************************
    wallet.api.transfer_to_vesting('initminer', 'alice', Asset.Test(500))

    #**************************************************************
    response = wallet.api.get_account('alice')

    check_withdraw_data( response['result'], '0.000000 VESTS', 0 )

    #**************************************************************
    response = wallet.api.get_withdraw_routes('alice', 'incoming')

    assert len(response['result']) == 0
    #**************************************************************
    wallet.api.withdraw_vesting('alice', '4.000000 VESTS')

    #**************************************************************
    response = wallet.api.get_account('alice')

    check_withdraw_data( response['result'], '0.307693 VESTS', 4000000 )

    #**************************************************************
    response = wallet.api.get_withdraw_routes('alice', 'incoming')

    assert len(response['result']) == 0

    #**************************************************************
    response = wallet.api.set_withdraw_vesting_route('alice', 'bob', 30, True )

    _value = check_route_data( response['result'] )
    check_route( _value, 'alice', 'bob', 30, True )

    #**************************************************************
    response = wallet.api.get_withdraw_routes('alice', 'outgoing')
    _result = response['result']

    assert len(_result) == 1

    check_route( _result[0], 'alice', 'bob', 30, True )

    #**************************************************************
    response = wallet.api.set_withdraw_vesting_route('alice', 'carol', 25, False )

    _value = check_route_data( response['result'] )
    check_route( _value, 'alice', 'carol', 25, False )

    #**************************************************************
    response = wallet.api.get_withdraw_routes('alice', 'outgoing')

    _result = response['result']

    assert len(_result) == 2

    check_route( _result[0], 'alice', 'bob', 30, True )
    check_route( _result[1], 'alice', 'carol', 25, False )
