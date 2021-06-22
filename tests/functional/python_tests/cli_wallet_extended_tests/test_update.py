from test_tools import Account, logger, World

def check_key( node_name, result, key ):
  assert node_name in result
  _node = result[node_name]
  assert 'key_auths' in _node
  _key_auths = _node['key_auths']
  assert len(_key_auths) == 1
  __key_auths = _key_auths[0]
  assert len(__key_auths) == 2
  __key_auths[0] == key

def test_update():
    with World() as world:
        init_node = world.create_init_node()
        init_node.run()

        wallet = init_node.attach_wallet()

        #**************************************************************
        logger.info('create_account...')
        response = wallet.api.create_account('initminer', 'alice', '{}')
        logger.info(response)

        #**************************************************************
        logger.info('transfer_to_vesting...')
        response = wallet.api.transfer_to_vesting('initminer', 'alice', '500.000 TESTS')
        logger.info(response)

        #**************************************************************
        logger.info('update_account_auth_account...')
        response = wallet.api.update_account_auth_account('alice', 'posting', 'initminer', 2)
        logger.info(response)

        #**************************************************************
        logger.info('get_account...')
        response = wallet.api.get_account('alice')
        logger.info(response)

        assert 'result' in response
        _result = response['result']

        assert 'posting' in _result
        _posting = _result['posting']
        assert 'account_auths' in _posting
        _account_auths = _posting['account_auths']
        assert len(_account_auths) == 1
        __account_auths = _account_auths[0]
        assert len(__account_auths) == 2
        __account_auths[0] == 'initminer'
        __account_auths[1] == 2

        #**************************************************************
        logger.info('update_account_auth_key...')
        response = wallet.api.update_account_auth_key('alice', 'posting', 'TST8ViK3T9FHbbtQs9Mo5odBM6tSmtFEVCvjEDKNPqKe9U1bJs53f', 3)
        logger.info(response)

        #**************************************************************
        logger.info('get_account...')
        response = wallet.api.get_account('alice')
        logger.info(response)

        assert 'result' in response
        _result = response['result']

        assert 'posting' in _result
        _posting = _result['posting']
        assert 'key_auths' in _posting
        _key_auths = _posting['key_auths']
        assert len(_key_auths) == 2
        __key_auths = _key_auths[1]
        assert len(__key_auths) == 2
        __account_auths[0] == 'TST8ViK3T9FHbbtQs9Mo5odBM6tSmtFEVCvjEDKNPqKe9U1bJs53f'
        __account_auths[1] == 3

        #**************************************************************
        logger.info('update_account_auth_threshold...')
        response = wallet.api.update_account_auth_threshold('alice', 'posting', 4)
        logger.info(response)

        #**************************************************************
        logger.info('get_account...')
        response = wallet.api.get_account('alice')
        logger.info(response)

        assert 'result' in response
        _result = response['result']

        assert 'posting' in _result
        _posting = _result['posting']
        assert 'key_auths' in _posting
        _key_auths = _posting['key_auths']
        assert len(_key_auths) == 2
        __key_auths = _key_auths[1]
        assert len(__key_auths) == 2
        __account_auths[1] == 4

        #**************************************************************
        logger.info('update_account_memo_key...')
        response = wallet.api.update_account_memo_key('alice', 'TST84oS1GW3yb9QaRaGztrqH5cHqeFFyLgLGSK4FoLEoDFBJqCnSJ')
        logger.info(response)

        #**************************************************************
        logger.info('get_account...')
        response = wallet.api.get_account('alice')
        logger.info(response)

        assert 'result' in response
        _result = response['result']

        assert 'memo_key' in _result
        _result['memo_key'] == 'TST84oS1GW3yb9QaRaGztrqH5cHqeFFyLgLGSK4FoLEoDFBJqCnSJ'

        #**************************************************************
        logger.info('update_account_meta...')
        response = wallet.api.update_account_meta('alice', '{ "test" : 4 }')
        logger.info(response)

        #**************************************************************
        logger.info('get_account...')
        response = wallet.api.get_account('alice')
        logger.info(response)

        assert 'result' in response
        _result = response['result']

        assert 'json_metadata' in _result
        _result['json_metadata'] == '{ "test" : 4 }'

        #**************************************************************
        logger.info('update_account...')
        key = 'TST8grZpsMPnH7sxbMVZHWEu1D26F3GwLW1fYnZEuwzT4Rtd57AER'
        response = wallet.api.update_account('alice', '{}', key, key, key, key)
        logger.info(response)

        #**************************************************************
        logger.info('get_account...')
        response = wallet.api.get_account('alice')
        logger.info(response)

        assert 'result' in response
        _result = response['result']

        check_key( 'owner', _result, key )
        check_key( 'active', _result, key )
        check_key( 'posting', _result, key )
        assert 'memo_key' in _result and _result['memo_key'] == key

        #**************************************************************
        logger.info('get_owner_history...')
        response = wallet.api.get_owner_history('alice')
        logger.info(response)

        assert 'result' in response
        _result = response['result']

        assert len(_result) == 1
        _data = _result[0]

        assert 'account' in _data
        assert _data['account'] == 'alice'
