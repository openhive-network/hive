from test_tools import Account, logger, World

def test_recovery(world):
    init_node = world.create_init_node()
    init_node.run()

    wallet = init_node.attach_wallet()

    def get_key( node_name, result):
      _node = result[node_name]
      _key_auths = _node['key_auths']
      assert len(_key_auths) == 1
      __key_auths = _key_auths[0]
      assert len(__key_auths) == 2
      return __key_auths[0]

    #**************************************************************
    response = wallet.api.create_account('initminer', 'alice', '{}')
    assert 'result' in response

    #**************************************************************
    response = wallet.api.transfer_to_vesting('initminer', 'alice', '100.000 TESTS')
    assert 'result' in response

    #**************************************************************
    response = wallet.api.transfer('initminer', 'alice', '500.000 TESTS', 'banana')
    assert 'result' in response

    #**************************************************************
    response = wallet.api.create_account('alice', 'bob', '{}')
    assert 'result' in response

    #**************************************************************
    response = wallet.api.transfer_to_vesting('alice', 'bob', '40.000 TESTS')
    assert 'result' in response

    #**************************************************************
    response = wallet.api.transfer('initminer', 'bob', '333.000 TESTS', 'kiwi-banana')
    assert 'result' in response

    #**************************************************************
    response = wallet.api.change_recovery_account('alice', 'bob')

    _result = response['result']
    _ops = _result['operations']

    assert len(_ops) == 1
    _op = _ops[0]

    assert _op[0] == 'change_recovery_account'
    _value = _op[1]

    assert _value['account_to_recover'] == 'alice'
    assert _value['new_recovery_account'] == 'bob'

    #**************************************************************
    response = wallet.api.get_account('alice')
    _result = response['result']
    alice_owner_key = get_key( 'owner', _result )

    #**************************************************************
    response = wallet.api.get_account('bob')
    _result = response['result']
    bob_owner_key = get_key( 'owner', _result )

    #**************************************************************
    authority = {"weight_threshold": 1,"account_auths": [], "key_auths": [[alice_owner_key,1]]}

    response = wallet.api.request_account_recovery('alice', 'bob', authority)

    _result = response['result']

    _ops = _result['operations']

    _op = _ops[0]

    assert _op[0] == 'request_account_recovery'

    _value = _op[1]

    assert _value['recovery_account'] == 'alice'
    assert _value['account_to_recover'] == 'bob'

    #**************************************************************
    response = wallet.api.update_account_auth_key('bob', 'owner', bob_owner_key, 3)
    assert 'result' in response

    #**************************************************************
    recent_authority = {"weight_threshold": 1,"account_auths": [], "key_auths": [[bob_owner_key,1]]}
    new_authority = {"weight_threshold": 1,"account_auths": [], "key_auths": [[alice_owner_key,1]]}

    response = wallet.api.recover_account('bob', recent_authority, new_authority)

    _result = response['result']

    _ops = _result['operations']

    _op = _ops[0]

    assert _op[0] == 'recover_account'

    _value = _op[1]
    assert _value['account_to_recover'] == 'bob'

