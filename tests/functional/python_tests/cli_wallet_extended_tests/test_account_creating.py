from test_tools import Account, logger, World

def test_account_creation():
    with World() as world:
        init_node = world.create_init_node()
        init_node.run()

        wallet = init_node.attach_wallet()

        #**************************************************************
        response = wallet.api.create_account('initminer', 'newaccount', '{}')

        _operations = response['result']['operations']
        _operation = _operations[0]
        assert _operation['value']['fee'] == '0.000 TESTS'

        _owner = _operation['value']['owner']
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
        response = wallet.api.list_accounts('na', 1)
        _result = response['result']

        assert _result[0] == 'newaccount'

        #**************************************************************
        response = wallet.api.get_account('newaccount')
        _result = response['result']

        assert _result['hbd_balance'] == '0.000 TBD'

        #**************************************************************
        response = wallet.api.get_account_history('initminer', 2, 2)
        #this call has a custom formatter so typical JSON is inaccessible
        assert len(response['result']) == 0

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
        response = wallet.api.follow('alice', 'bob', ['blog'])
        _result = response['result']

        _ops = _result['operations']
        _op = _ops[0]

        assert _op['type'] == 'custom_json_operation'

        _value = _op['value']
        assert _value['json'] == '{"type":"follow_operation","value":{"follower":"alice","following":"@bob","what":["blog"]}}'
