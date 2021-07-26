def test_account_creation(wallet):
    wallet.api.create_account('initminer', 'alice', '{}')

    response = wallet.api.list_accounts('a', 1)
    assert response['result'] == ['alice']


def test_account_creation_and_much_more(wallet):
    #**************************************************************
    response = wallet.api.create_account('initminer', 'newaccount', '{}')

    _operations = response['result']['operations']
    _operation = _operations[0]
    assert _operation[1]['fee'] == '0.000 TESTS'

    _owner = _operation[1]['owner']
    _key_auths = _owner['key_auths']
    __key_auths = _key_auths[0]
    assert len(__key_auths) == 2
    owner_key = __key_auths[0]

    #**************************************************************
    response = wallet.api.list_my_accounts([owner_key])
    _result = response['result']

    assert _result[0]['balance'] == '0.000 TESTS'
    assert _result[0]['savings_balance'] == '0.000 TESTS'

    #**************************************************************
    response = wallet.api.get_account('newaccount')
    _result = response['result']

    assert _result['hbd_balance'] == '0.000 TBD'

    #**************************************************************
    response = wallet.api.get_account_history('initminer', 2, 2)
    #this call has a custom formatter so typical JSON is inaccessible
    assert len(response['result']) == 0
