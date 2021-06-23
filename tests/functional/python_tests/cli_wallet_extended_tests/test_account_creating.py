from test_tools import Account, logger, World

def test_account_creation():
    with World() as world:
        init_node = world.create_init_node()
        init_node.run()

        wallet = init_node.attach_wallet()

        #**************************************************************
        logger.info('create_account...')
        response = wallet.api.create_account('initminer', 'newaccount', '{}')
        logger.info(response)

        _operations = response['result']['operations']
        assert len(_operations) == 1

        _operation = _operations[0]
        assert 'value' in _operation
        assert 'fee' in _operation['value']
        assert _operation['value']['fee'] == '0.000 TESTS'

        assert 'owner' in _operation['value']
        _owner = _operation['value']['owner']
        assert 'key_auths' in _owner
        _key_auths = _owner['key_auths']
        assert len(_key_auths) == 1
        __key_auths = _key_auths[0]
        assert len(__key_auths) == 2
        owner_key = __key_auths[0]

        #**************************************************************
        logger.info('list_my_accounts...')
        response = wallet.api.list_my_accounts([owner_key])
        logger.info(response)

        assert len(response['result']) == 1
        _result = response['result'][0]
        assert 'balance' in _result
        assert _result['balance'] == '0.000 TESTS'
        assert 'savings_balance' in _result
        assert _result['savings_balance'] == '0.000 TESTS'

        #**************************************************************
        logger.info('list_accounts...')
        response = wallet.api.list_accounts('na', 1)
        logger.info(response)
        assert len(response['result']) == 1
        assert response['result'][0] == 'newaccount'

        #**************************************************************
        logger.info('get_account...')
        response = wallet.api.get_account('newaccount')
        logger.info(response)
        _result = response['result']
        assert 'hbd_balance' in _result
        assert _result['hbd_balance'] == '0.000 TBD'

        #**************************************************************
        logger.info('get_account_history...')
        response = wallet.api.get_account_history('initminer', 2, 2)
        logger.info(response)
        #this call has a custom formatter so typical JSON is inaccessible
        assert len(response['result']) == 0

        #**************************************************************
        logger.info('create_account...')
        response = wallet.api.create_account('initminer', 'alice', '{}')
        logger.info(response)

        #**************************************************************
        logger.info('create_account...')
        response = wallet.api.create_account('initminer', 'bob', '{}')
        logger.info(response)

        #**************************************************************
        logger.info('transfer_to_vesting...')
        response = wallet.api.transfer_to_vesting('initminer', 'alice', '100.000 TESTS')
        logger.info(response)

        assert 'result' in response

        #**************************************************************
        logger.info('follow...')
        response = wallet.api.follow('alice', 'bob', ['blog'])
        logger.info(response)

        _ops = response['result']['operations']

        assert len(_ops) == 1
        _op = _ops[0]

        assert 'type' in _op and _op['type'] == 'custom_json_operation'

        assert 'value' in _op
        _value = _op['value']

        assert 'json' in _value and _value['json'] == '{"type":"follow_operation","value":{"follower":"alice","following":"@bob","what":["blog"]}}'
