from test_tools import Account, logger, World

def test_recovery():
    with World() as world:
        init_node = world.create_init_node()
        init_node.run()

        wallet = init_node.attach_wallet()

        def get_key( node_name, result):
          assert node_name in result
          _node = result[node_name]
          assert 'key_auths' in _node
          _key_auths = _node['key_auths']
          assert len(_key_auths) == 1
          __key_auths = _key_auths[0]
          assert len(__key_auths) == 2
          return __key_auths[0]

        #**************************************************************
        logger.info('create_account...')
        response = wallet.api.create_account('initminer', 'alice', '{}')
        logger.info(response)

        assert 'result' in response

        #**************************************************************
        logger.info('transfer_to_vesting...')
        response = wallet.api.transfer_to_vesting('initminer', 'alice', '100.000 TESTS')
        logger.info(response)

        assert 'result' in response

        #**************************************************************
        logger.info('transfer...')
        response = wallet.api.transfer('initminer', 'alice', '500.000 TESTS', 'banana')
        logger.info(response)

        assert 'result' in response

        #**************************************************************
        logger.info('create_account...')
        response = wallet.api.create_account('alice', 'bob', '{}')
        logger.info(response)

        assert 'result' in response

        #**************************************************************
        logger.info('transfer_to_vesting...')
        response = wallet.api.transfer_to_vesting('alice', 'bob', '40.000 TESTS')
        logger.info(response)

        assert 'result' in response

        #**************************************************************
        logger.info('transfer...')
        response = wallet.api.transfer('initminer', 'bob', '333.000 TESTS', 'kiwi-banana')
        logger.info(response)

        assert 'result' in response

        #**************************************************************
        logger.info('change_recovery_account...')
        response = wallet.api.change_recovery_account('alice', 'bob')
        logger.info(response)

        _result = response['result']

        assert 'operations' in _result
        _ops = _result['operations']

        assert len(_ops) == 1
        _op = _ops[0]

        assert 'type' in _op and _op['type'] == 'change_recovery_account_operation'

        assert 'value' in _op
        _value = _op['value']

        assert 'account_to_recover' in _value and _value['account_to_recover'] == 'alice'
        assert 'new_recovery_account' in _value and _value['new_recovery_account'] == 'bob'

        #**************************************************************
        logger.info('get_account...')
        response = wallet.api.get_account('alice')
        logger.info(response)

        _result = response['result']

        alice_owner_key = get_key( 'owner', _result )

        #**************************************************************
        logger.info('get_account...')
        response = wallet.api.get_account('bob')
        logger.info(response)

        _result = response['result']

        bob_owner_key = get_key( 'owner', _result )

        #**************************************************************
        logger.info('request_account_recovery...')
        authority = {"weight_threshold": 1,"account_auths": [], "key_auths": [[alice_owner_key,1]]}

        response = wallet.api.request_account_recovery('alice', 'bob', authority)
        logger.info(response)

        _result = response['result']

        assert 'operations' in _result
        _ops = _result['operations']

        assert len(_ops) == 1
        _op = _ops[0]

        assert 'type' in _op and _op['type'] == 'request_account_recovery_operation'

        assert 'value' in _op
        _value = _op['value']

        assert 'recovery_account' in _value and _value['recovery_account'] == 'alice'
        assert 'account_to_recover' in _value and _value['account_to_recover'] == 'bob'

        #**************************************************************
        logger.info('update_account_auth_key...')
        response = wallet.api.update_account_auth_key('bob', 'owner', bob_owner_key, 3)
        logger.info(response)

        #**************************************************************
        logger.info('recover_account...')
        recent_authority = {"weight_threshold": 1,"account_auths": [], "key_auths": [[bob_owner_key,1]]}
        new_authority = {"weight_threshold": 1,"account_auths": [], "key_auths": [[alice_owner_key,1]]}

        response = wallet.api.recover_account('bob', recent_authority, new_authority)
        logger.info(response)

        _result = response['result']

        assert 'operations' in _result
        _ops = _result['operations']

        assert len(_ops) == 1
        _op = _ops[0]

        assert 'type' in _op and _op['type'] == 'recover_account_operation'

        assert 'value' in _op
        _value = _op['value']

        assert 'account_to_recover' in _value and _value['account_to_recover'] == 'bob'
