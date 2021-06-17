from test_tools import Account, logger, World
import os.path

def test_getters():
    with World() as world:
        init_node = world.create_init_node()
        init_node.config.plugin.append('database_api')
        init_node.run()

        wallet = init_node.attach_wallet()

        pswd = 'pear_peach'

        internal_path = 'GeneratedInWorld/InitNodeWallet0/'
        wallet_content_file_name = 'test_wallet.json'

        #**************************************************************
        logger.info('save_wallet_file...')
        response = wallet.api.save_wallet_file(wallet_content_file_name)
        logger.info(response)

        assert 'result' in response
        response['result'] = None

        assert os.path.isfile(internal_path + wallet_content_file_name)

        #**************************************************************
        logger.info('load_wallet_file...')
        response = wallet.api.save_wallet_file(internal_path + wallet_content_file_name)
        logger.info(response)

        assert 'result' in response
        response['result'] = None

        #**************************************************************
        logger.info('is_new...')
        response = wallet.api.is_new()
        logger.info(response)

        assert 'result' in response
        response['result'] = False

        #**************************************************************
        logger.info('is_locked...')
        response = wallet.api.is_locked()
        logger.info(response)

        assert 'result' in response
        response['result'] = False

        #**************************************************************
        logger.info('set_password...')
        response = wallet.api.set_password(pswd)
        logger.info(response)

        assert 'result' in response
        response['result'] = None

        #**************************************************************
        logger.info('is_locked...')
        response = wallet.api.is_locked()
        logger.info(response)

        assert 'result' in response
        response['result'] = True

        #**************************************************************
        logger.info('unlock...')
        response = wallet.api.unlock(pswd)
        logger.info(response)

        assert 'result' in response
        response['result'] = None

        #**************************************************************
        logger.info('is_locked...')
        response = wallet.api.is_locked()
        logger.info(response)

        assert 'result' in response
        response['result'] = False

        #**************************************************************
        logger.info('lock...')
        response = wallet.api.lock()
        logger.info(response)

        assert 'result' in response
        response['result'] = None

        #**************************************************************
        logger.info('is_locked...')
        response = wallet.api.is_locked()
        logger.info(response)

        assert 'result' in response
        response['result'] = True

        #**************************************************************
        logger.info('unlock...')
        response = wallet.api.unlock(pswd)
        logger.info(response)

        assert 'result' in response
        response['result'] = None

        #**************************************************************
        logger.info('is_locked...')
        response = wallet.api.is_locked()
        logger.info(response)

        assert 'result' in response
        response['result'] = False

        #**************************************************************
        logger.info('list_keys...')
        response = wallet.api.list_keys()
        logger.info(response)

        assert 'result' in response
        _result = response['result']

        assert len(_result) == 1
        _keys = _result[0]

        assert len(_keys) == 2
        assert _keys[0] == 'TST6LLegbAgLAy28EHrffBVuANFWcFgmqRMW13wBmTExqFE9SCkg4'
        assert _keys[1] == '5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n'

        #**************************************************************
        logger.info('import_key...')
        response = wallet.api.import_key('5JE4eBgPiRiVcdcpJ8tQpMpm6dgm1uAuq9Kn2nn1M9xK94EE5nU')
        logger.info(response)

        assert 'result' in response
        _result = response['result']

        #**************************************************************
        logger.info('list_keys...')
        response = wallet.api.list_keys()
        logger.info(response)

        assert 'result' in response
        _result = response['result']

        assert len(_result) == 2
        _keys = _result[1]

        assert len(_keys) == 2
        assert _keys[0] == 'TST8LRQW8NXuDnextVpzKA5Fp47k591mHJjKnfTMdfQvjR5qA1yqK'
        assert _keys[1] == '5JE4eBgPiRiVcdcpJ8tQpMpm6dgm1uAuq9Kn2nn1M9xK94EE5nU'

        #**************************************************************
        logger.info('get_private_key_from_password...')
        response = wallet.api.get_private_key_from_password('hulabula', 'owner', "apricot")
        logger.info(response)

        assert 'result' in response
        _result = response['result']

        assert len(_result) == 2

        assert _result[0] == 'TST5Fuu7PnmJh5dxguaxMZU1KLGcmAh8xgg3uGMUmV9m62BDQb3kB'
        assert _result[1] == '5HwfhtUXPdxgwukwfjBbwogWfaxrUcrJk6u6oCfv4Uw6DZwqC1H'

        #**************************************************************
        logger.info('get_private_key...')
        response = wallet.api.get_private_key('TST6LLegbAgLAy28EHrffBVuANFWcFgmqRMW13wBmTExqFE9SCkg4')
        logger.info(response)

        assert 'result' in response
        _result = response['result']

        assert _result == '5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n'

        #**************************************************************
        logger.info('gethelp...')
        response = wallet.api.gethelp('find_proposals')
        logger.info(response)

        assert 'result' in response
        _result = response['result']

        found = _result.find('Find proposal with given id')
        assert found != -1

        #**************************************************************
        logger.info('gethelp...')
        response = wallet.api.gethelp('create_proposal')
        logger.info(response)

        assert 'result' in response
        _result = response['result']

        found = _result.find('Create worker proposal')
        assert found != -1

        #**************************************************************
        logger.info('help...')
        response = wallet.api.help()
        logger.info(response)

        assert 'result' in response
        _result = response['result']

        val = 'serializer_wrapper<annotated_signed_transaction> cancel_transfer_from_savings(const string & from, uint32_t request_id, bool broadcast)'
        found = _result.find(val)
        assert found != -1

        val = 'serializer_wrapper<annotated_signed_transaction> convert_hbd(const string & from, const hive::protocol::legacy_asset & amount, bool broadcast)'
        found = _result.find(val)
        assert found != -1

        #**************************************************************
        logger.info('info...')
        response = wallet.api.info()
        logger.info(response)

        assert 'result' in response
        _result = response['result']

        assert 'median_hbd_price' in _result
        _median_hbd_price = _result['median_hbd_price']

        assert 'base' in _median_hbd_price and _median_hbd_price['base'] == '0.001 TBD'
        assert 'quote' in _median_hbd_price and _median_hbd_price['quote'] == '0.001 TESTS'

        assert 'account_creation_fee' in _result and _result['account_creation_fee'] == '0.000 TESTS'

        assert 'post_reward_fund' in _result
        _post_reward_fund = _result['post_reward_fund']

        assert 'reward_balance' in _post_reward_fund and _post_reward_fund['reward_balance'] != '0.000 TESTS'

        #**************************************************************
        logger.info('normalize_brain_key...')
        response = wallet.api.normalize_brain_key('     mango apple banana CHERRY ')
        logger.info(response)

        assert 'result' in response and response['result'] == 'MANGO APPLE BANANA CHERRY'

        #**************************************************************
        logger.info('suggest_brain_key...')
        response = wallet.api.suggest_brain_key()
        logger.info(response)

        assert 'result' in response
        _result = response['result']

        assert 'brain_priv_key' in _result
        _brain_priv_key = _result['brain_priv_key']
        _items = _brain_priv_key.split(' ')
        assert len(_items) == 16

        assert 'wif_priv_key' in _result and len(_result['wif_priv_key']) == 51
        assert 'pub_key' in _result and _result['pub_key'].find('TST') != -1
