from test_tools import Account, logger, World

def test_account_creation():
    with World() as world:
        init_node = world.create_init_node()
        init_node.config.plugin.append('database_api')
        init_node.config.plugin.append('network_broadcast_api')
        init_node.run()

        wallet = init_node.attach_wallet()

        logger.info('Waiting until initminer will be able to create account...')
        init_node.wait_number_of_blocks(3)

        #**************************************************************
        response = wallet.api.create_account('initminer', 'newaccount', '{}', True)
        logger.info(response)

        assert 'result' in response
        assert 'operations' in response['result']

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
        response = wallet.api.list_my_accounts([owner_key])
        logger.info(response)

        assert 'result' in response
        assert len(response['result']) == 1
        _result = response['result'][0]
        assert 'balance' in _result
        _result['balance'] == '0.000 TESTS'
        assert 'savings_balance' in _result
        _result['savings_balance'] == '0.000 TESTS'

        #**************************************************************
        response = wallet.api.list_accounts('na', 1)
        logger.info(response)
        assert 'result' in response
        assert len(response['result']) == 1
        assert response['result'][0] == 'newaccount'