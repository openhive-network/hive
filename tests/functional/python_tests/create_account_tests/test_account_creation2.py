from test_tools import Account, logger, World

def test_account_creation():
    with World() as world:
        init_node = world.create_init_node()
        init_node.config.plugin.append('database_api')
        init_node.config.plugin.append('network_broadcast_api')
        init_node.config.plugin.append('account_history_rocksdb')
        init_node.config.plugin.append('account_history')
        init_node.config.plugin.append('account_history_api')
        init_node.run()

        wallet = init_node.attach_wallet()

        #**************************************************************
        logger.info('list_accounts...')
        response = wallet.api.list_accounts('a', 100)
        logger.info(response)
        assert 'result' in response
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
        assert 'result' in response
        old_accounts_number + 2 == len(response['result'])