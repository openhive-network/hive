from test_tools import Account, logger, World, Asset

def test_recovery(wallet):

    def get_key( node_name, result):
      _node = result[node_name]
      _key_auths = _node['key_auths']
      assert len(_key_auths) == 1
      __key_auths = _key_auths[0]
      assert len(__key_auths) == 2
      return __key_auths[0]

    #**************************************************************
    wallet.api.create_account('initminer', 'alice', '{}')

    #**************************************************************
    wallet.api.transfer_to_vesting('initminer', 'alice', Asset.Test(100))

    #**************************************************************
    wallet.api.transfer('initminer', 'alice', Asset.Test(500), 'banana')

    #**************************************************************
    wallet.api.create_account('alice', 'bob', '{}')

    #**************************************************************
    wallet.api.transfer_to_vesting('alice', 'bob', Asset.Test(40))

    #**************************************************************
    wallet.api.transfer('initminer', 'bob', Asset.Test(333), 'kiwi-banana')

    #**************************************************************
    response = wallet.api.change_recovery_account('alice', 'bob')

    _ops = response['result']['operations']

    assert len(_ops) == 1
    _op = _ops[0]

    assert _op[0] == 'change_recovery_account'
    _value = _op[1]

    assert _value['account_to_recover'] == 'alice'
    assert _value['new_recovery_account'] == 'bob'

    #**************************************************************
    response = wallet.api.get_account('alice')
    alice_owner_key = get_key( 'owner', response['result'] )

    #**************************************************************
    response = wallet.api.get_account('bob')
    bob_owner_key = get_key( 'owner', response['result'] )

    #**************************************************************
    authority = {"weight_threshold": 1,"account_auths": [], "key_auths": [[alice_owner_key,1]]}

    response = wallet.api.request_account_recovery('alice', 'bob', authority)

    _ops = response['result']['operations']

    _op = _ops[0]

    assert _op[0] == 'request_account_recovery'

    _value = _op[1]

    assert _value['recovery_account'] == 'alice'
    assert _value['account_to_recover'] == 'bob'

    #**************************************************************
    wallet.api.update_account_auth_key('bob', 'owner', bob_owner_key, 3)

    #**************************************************************
    recent_authority = {"weight_threshold": 1,"account_auths": [], "key_auths": [[bob_owner_key,1]]}
    new_authority = {"weight_threshold": 1,"account_auths": [], "key_auths": [[alice_owner_key,1]]}

    response = wallet.api.recover_account('bob', recent_authority, new_authority)

    _ops = response['result']['operations']

    _op = _ops[0]

    assert _op[0] == 'recover_account'

    _value = _op[1]
    assert _value['account_to_recover'] == 'bob'

