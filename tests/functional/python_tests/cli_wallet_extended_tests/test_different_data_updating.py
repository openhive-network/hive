from test_tools import Account, logger, World, Asset

def check_key( node_name, result, key ):
  _node = result[node_name]
  _key_auths = _node['key_auths']
  assert len(_key_auths) == 1
  __key_auths = _key_auths[0]
  assert len(__key_auths) == 2
  __key_auths[0] == key

def test_update(world):
    init_node = world.create_init_node()
    init_node.run()

    wallet = init_node.attach_wallet()

    #**************************************************************
    response = wallet.api.create_account('initminer', 'alice', '{}')
    assert 'result' in response

    #**************************************************************
    response = wallet.api.transfer_to_vesting('initminer', 'alice', Asset.Test(500))
    assert 'result' in response

    #**************************************************************
    response = wallet.api.update_account_auth_account('alice', 'posting', 'initminer', 2)
    assert 'result' in response

    #**************************************************************
    response = wallet.api.get_account('alice')
    _result = response['result']

    _posting = _result['posting']
    _account_auths = _posting['account_auths']
    assert len(_account_auths) == 1
    __account_auths = _account_auths[0]
    assert len(__account_auths) == 2
    assert __account_auths[0] == 'initminer'
    assert __account_auths[1] == 2

    #**************************************************************
    response = wallet.api.update_account_auth_key('alice', 'posting', 'TST8ViK3T9FHbbtQs9Mo5odBM6tSmtFEVCvjEDKNPqKe9U1bJs53f', 3)
    assert 'result' in response

    #**************************************************************
    response = wallet.api.get_account('alice')
    _result = response['result']

    _posting = _result['posting']
    _key_auths = _posting['key_auths']
    assert len(_key_auths) == 2
    __key_auths = _key_auths[1]
    assert len(__key_auths) == 2
    __account_auths[0] == 'TST8ViK3T9FHbbtQs9Mo5odBM6tSmtFEVCvjEDKNPqKe9U1bJs53f'
    __account_auths[1] == 3

    #**************************************************************
    response = wallet.api.update_account_auth_threshold('alice', 'posting', 4)
    assert 'result' in response

    #**************************************************************
    response = wallet.api.get_account('alice')
    _result = response['result']

    _posting = _result['posting']
    _key_auths = _posting['key_auths']
    assert len(_key_auths) == 2
    __key_auths = _key_auths[1]
    assert len(__key_auths) == 2
    __account_auths[1] == 4

    #**************************************************************
    response = wallet.api.update_account_memo_key('alice', 'TST84oS1GW3yb9QaRaGztrqH5cHqeFFyLgLGSK4FoLEoDFBJqCnSJ')
    assert 'result' in response

    #**************************************************************
    response = wallet.api.get_account('alice')
    _result = response['result']

    assert _result['memo_key'] == 'TST84oS1GW3yb9QaRaGztrqH5cHqeFFyLgLGSK4FoLEoDFBJqCnSJ'

    #**************************************************************
    response = wallet.api.update_account_meta('alice', '{ "test" : 4 }')
    _result = response['result']

    #**************************************************************
    response = wallet.api.get_account('alice')
    _result = response['result']

    assert _result['json_metadata'] == '{ "test" : 4 }'

    #**************************************************************
    key = 'TST8grZpsMPnH7sxbMVZHWEu1D26F3GwLW1fYnZEuwzT4Rtd57AER'
    response = wallet.api.update_account('alice', '{}', key, key, key, key)
    assert 'result' in response

    #**************************************************************
    response = wallet.api.get_account('alice')
    _result = response['result']

    check_key( 'owner', _result, key )
    check_key( 'active', _result, key )
    check_key( 'posting', _result, key )
    assert _result['memo_key'] == key

    #**************************************************************
    response = wallet.api.get_owner_history('alice')
    _result = response['result']

    assert len(_result) == 1
    _data = _result[0]

    assert 'account' in _data
    assert _data['account'] == 'alice'
