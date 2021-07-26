def test_following(wallet):
    # **************************************************************
    response = wallet.api.create_account('initminer', 'alice', '{}')
    assert 'result' in response

    # **************************************************************
    response = wallet.api.create_account('initminer', 'bob', '{}')
    assert 'result' in response

    # **************************************************************
    response = wallet.api.transfer_to_vesting('initminer', 'alice', '100.000 TESTS')
    assert 'result' in response

    # **************************************************************
    response = wallet.api.follow('alice', 'bob', ['blog'])
    _result = response['result']

    _ops = _result['operations']
    _op = _ops[0]

    assert _op[0] == 'custom_json'

    _value = _op[1]
    assert _value['json'] == '{"type":"follow_operation","value":{"follower":"alice","following":"@bob","what":["blog"]}}'
