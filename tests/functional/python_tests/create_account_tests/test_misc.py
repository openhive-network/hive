from test_tools import Account, logger, World

def test_transfer():
    with World() as world:
        init_node = world.create_init_node()
        init_node.config.plugin.append('database_api')
        init_node.config.plugin.append('network_broadcast_api')
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
        _result = response['result']

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
        _result = response['result']

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
        logger.info('about...')
        response = wallet.api.about()
        logger.info(response)

        assert 'result' in response
        _result = response['result']

        assert 'blockchain_version' in _result
        assert 'client_version' in _result
        assert 'hive_revision' in _result
        assert 'hive_revision_age' in _result
        assert 'fc_revision' in _result
        assert 'fc_revision_age' in _result
        assert 'compile_date' in _result
        assert 'boost_version' in _result
        assert 'openssl_version' in _result
        assert 'build' in _result
        assert 'server_blockchain_version' in _result
        assert 'server_hive_revision' in _result
        assert 'server_fc_revision' in _result

        #**************************************************************
        logger.info('get_account...')
        response = wallet.api.get_account('initminer')
        logger.info(response)

        assert 'result' in response
        _result = response['result']

        #**************************************************************
        try:
            logger.info('claim_reward_balance...')
            response = wallet.api.claim_reward_balance('initminer', '0.000 TESTS', '0.000 TBD', '0.000001 VESTS')
            logger.info(response)
        except Exception as e:
            message = str(e)
            logger.info(message)
            found = message.find('Cannot claim that much VESTS')
            assert found != -1

        #**************************************************************
        logger.info('decline_voting_rights...')
        response = wallet.api.decline_voting_rights('alice', True)
        logger.info(response)

        assert 'result' in response
        _result = response['result']

        assert 'operations' in _result
        _ops = _result['operations']

        assert len(_ops) == 1
        _op = _ops[0]

        assert 'type' in _op and _op['type'] == 'decline_voting_rights_operation'

        assert 'value' in _op
        _value = _op['value']

        assert 'account' in _value and _value['account'] == 'alice'

        #**************************************************************
        logger.info('follow...')
        response = wallet.api.follow('alice', 'bob', ['blog'])
        logger.info(response)

        assert 'result' in response
        _result = response['result']

        assert 'operations' in _result
        _ops = _result['operations']

        assert len(_ops) == 1
        _op = _ops[0]

        assert 'type' in _op and _op['type'] == 'custom_json_operation'

        assert 'value' in _op
        _value = _op['value']

        assert 'json' in _value and _value['json'] == '{"type":"follow_operation","value":{"follower":"alice","following":"@bob","what":["blog"]}}'
