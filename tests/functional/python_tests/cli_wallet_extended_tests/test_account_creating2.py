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

def test_account_creation2():
    with World() as world:
        init_node = world.create_init_node()
        init_node.run()

        wallet = init_node.attach_wallet()

        #**************************************************************
        logger.info('list_accounts...')
        response = wallet.api.list_accounts('a', 100)
        logger.info(response)
        old_accounts_number = len(response['result'])

        logger.info('Waiting...')
        init_node.wait_number_of_blocks(12)
       
        #**************************************************************
        logger.info('claim_account_creation...')
        response = wallet.api.claim_account_creation('initminer', '0.000 TESTS')
        logger.info(response)

        #**************************************************************
        logger.info('claim_account_creation_nonblocking...')
        response = wallet.api.claim_account_creation_nonblocking('initminer', '0.000 TESTS')
        logger.info(response)

        #**************************************************************
        try:
            logger.info('create_account_delegated...')
            response = wallet.api.create_account_delegated('initminer', '2.987 TESTS', '6.123456 VESTS', 'alicex', '{}')
            logger.info(response)
        except Exception as e:
            message = str(e)
            found = message.find('Account creation with delegation is deprecated as of Hardfork 20')
            assert found != -1

        #**************************************************************
        logger.info('create_account_with_keys...')
        key = 'TST8grZpsMPnH7sxbMVZHWEu1D26F3GwLW1fYnZEuwzT4Rtd57AER'
        response = wallet.api.create_account_with_keys('initminer', 'alice1', '{}', key, key, key, key)
        logger.info(response)

        _operations = response['result']['operations']
        assert len(_operations) == 1
        _value = _operations[0]['value']

        check_key( 'owner', _value, key )
        check_key( 'active', _value, key )
        check_key( 'posting', _value, key )
        assert _value['memo_key'] == key

        #**************************************************************
        try:
            logger.info('create_account_with_keys_delegated...')
            key = 'TST8grZpsMPnH7sxbMVZHWEu1D26F3GwLW1fYnZEuwzT4Rtd57AER'
            response = wallet.api.create_account_with_keys_delegated('initminer', '2.987 TESTS', '6.123456 VESTS', 'alicey', '{}', key, key, key, key)
            logger.info(response)
        except Exception as e:
            message = str(e)
            found = message.find('Account creation with delegation is deprecated as of Hardfork 20')
            assert found != -1

        #**************************************************************
        logger.info('create_funded_account_with_keys...')
        key = 'TST8grZpsMPnH7sxbMVZHWEu1D26F3GwLW1fYnZEuwzT4Rtd57AER'
        response = wallet.api.create_funded_account_with_keys('initminer', 'alice2', '2.456 TESTS', 'banana', '{}', key, key, key, key)
        logger.info(response)

        #**************************************************************
        logger.info('list_accounts...')
        response = wallet.api.list_accounts('a', 100)
        logger.info(response)
        old_accounts_number + 2 == len(response['result'])
