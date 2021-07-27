from test_tools import Account, logger, World, Asset

def test_comment(world):
    init_node = world.create_init_node()
    init_node.run()

    wallet = init_node.attach_wallet()

    #**************************************************************
    wallet.api.create_account('initminer', 'alice', '{}')

    #**************************************************************
    wallet.api.transfer('initminer', 'alice', Asset.Test(200), 'avocado')

    #**************************************************************
    wallet.api.transfer('initminer', 'alice', Asset.Tbd(100), 'banana')

    #**************************************************************
    wallet.api.transfer_to_vesting('initminer', 'alice', Asset.Test(50))

    #**************************************************************
    wallet.api.create_account('alice', 'bob', '{}')

    #**************************************************************
    wallet.api.transfer('alice', 'bob', Asset.Test(50), 'lemon')

    #**************************************************************
    wallet.api.transfer_to_vesting('alice', 'bob', Asset.Test(25))

    #**************************************************************
    response = wallet.api.post_comment('alice', 'test-permlink', '', 'xyz', 'śćą', 'DEBUG    test_tools.wallet.World.InitNodeWallet0:wallet.py:462 Closed with 0 return code', '{}')
    _result = response['result']

    _ops = _result['operations']
    _op = _ops[0]

    assert _op[0] == 'comment'

    _value = _op[1]

    assert _value['author'] == 'alice'
    assert _value['title'] == 'u015bu0107u0105'
    assert _value['body'] == 'DEBUG    test_tools.wallet.World.InitNodeWallet0:wallet.py:462 Closed with 0 return code'

    #**************************************************************
    response = wallet.api.post_comment('alice', 'test-permlink', '', 'xyz', 'TITLE NR 2', 'BODY NR 2', '{}')
    _result = response['result']

    _ops = _result['operations']
    _op = _ops[0]

    assert _op[0] == 'comment'

    _value = _op[1]

    assert _value['author'] == 'alice'
    assert _value['title'] == 'TITLE NR 2'
    assert _value['body'] == 'BODY NR 2'

    #**************************************************************
    response = wallet.api.post_comment('bob', 'bob-permlink', 'alice', 'test-permlink', 'TITLE NR 3', 'BODY NR 3', '{}')
    _result = response['result']

    _ops = _result['operations']
    _op = _ops[0]

    assert _op[0] == 'comment'

    _value = _op[1]

    assert _value['author'] == 'bob'
    assert _value['parent_author'] == 'alice'
    assert _value['title'] == 'TITLE NR 3'
    assert _value['body'] == 'BODY NR 3'

    #**************************************************************
    response = wallet.api.vote('bob', 'bob', 'bob-permlink', 100)
    _result = response['result']

    _ops = _result['operations']
    _op = _ops[0]

    _op[0] == 'vote'

    _value = _op[1]

    assert _value['voter'] == 'bob'
    assert _value['author'] == 'bob'
    assert _value['permlink'] == 'bob-permlink'
    assert _value['weight'] == 10000

    #**************************************************************
    response = wallet.api.decline_voting_rights('alice', True)
    _result = response['result']

    _ops = _result['operations']
    _op = _ops[0]

    assert _op[0] == 'decline_voting_rights'

    _value = _op[1]

    assert _value['account'] == 'alice'
