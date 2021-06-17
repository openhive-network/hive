from test_tools import Account, logger, World

def test_update():
    with World() as world:
        init_node = world.create_init_node()
        init_node.config.plugin.append('database_api')
        init_node.config.plugin.append('network_broadcast_api')
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
        logger.info('get_active_witnesses...')
        response = wallet.api.get_active_witnesses()
        logger.info(response)

        assert 'result' in response
        _result = response['result']

        assert len(_result) > 1
        assert _result[0] == 'initminer'
        assert _result[1] == ''

        #**************************************************************
        logger.info('list_witnesses...')
        response = wallet.api.list_witnesses('a', 4)
        logger.info(response)

        assert 'result' in response
        _result = response['result']

        assert len(_result) == 1
        assert _result[0] == 'initminer'
        #**************************************************************
        logger.info('update_witness...')
        response = wallet.api.update_witness('alice', 'http:\\url.html', 'TST6LLegbAgLAy28EHrffBVuANFWcFgmqRMW13wBmTExqFE9SCkg4', { 'account_creation_fee':'2789.000 TESTS', 'maximum_block_size' : 131072, 'hbd_interest_rate' : 1000 } )
        logger.info(response)

        #**************************************************************
        logger.info('get_active_witnesses...')
        response = wallet.api.get_active_witnesses()
        logger.info(response)

        assert 'result' in response
        _result = response['result']

        assert len(_result) > 1
        assert _result[0] == 'initminer'
        assert _result[1] == ''

        #**************************************************************
        logger.info('list_witnesses...')
        response = wallet.api.list_witnesses('a', 4)
        logger.info(response)

        assert 'result' in response
        _result = response['result']

        assert len(_result) == 2
        assert _result[0] == 'alice'
        assert _result[1] == 'initminer'
        #**************************************************************
        logger.info('get_witness...')
        response = wallet.api.get_witness('alice')
        logger.info(response)

        assert 'result' in response
        _result = response['result']

        assert 'owner' in _result
        assert _result['owner'] == 'alice'

        assert 'props' in _result
        _props = _result['props']

        assert 'account_creation_fee' in _props
        _props['account_creation_fee'] == '2.789 TESTS'

        #**************************************************************
        logger.info('publish_feed...')
        response = wallet.api.publish_feed('alice', {"base":"1.167 TBD", "quote":"1.111 TESTS"})
        logger.info(response)

        assert 'result' in response
        _result = response['result']

        assert 'operations' in _result
        _ops = _result['operations']

        assert len(_ops) == 1
        _op = _ops[0]

        assert 'type' in _op
        _op['type'] == 'feed_publish_operation'

        assert 'value' in _op
        _value = _op['value']

        assert 'exchange_rate' in _value
        _exchange_rate = _value['exchange_rate']

        assert 'base' in _exchange_rate and _exchange_rate['base'] == '1.167 TBD'
        assert 'quote' in _exchange_rate and _exchange_rate['quote'] == '1.111 TESTS'

        #**************************************************************
        logger.info('vote_for_witness...')
        response = wallet.api.vote_for_witness('initminer', 'alice', True)
        logger.info(response)

        assert 'result' in response
        _result = response['result']

        assert 'operations' in _result
        _ops = _result['operations']

        assert len(_ops) == 1
        _op = _ops[0]

        assert 'type' in _op
        _op['type'] == 'account_witness_vote_operation'

        assert 'value' in _op
        _value = _op['value']

        assert 'account' in _value and _value['account'] == 'initminer'
        assert 'witness' in _value and _value['witness'] == 'alice'
        assert 'approve' in _value and _value['approve'] == True

        #**************************************************************
        logger.info('set_voting_proxy...')
        response = wallet.api.set_voting_proxy('alice', 'initminer')
        logger.info(response)

        assert 'result' in response
        _result = response['result']

        assert 'operations' in _result
        _ops = _result['operations']

        assert len(_ops) == 1
        _op = _ops[0]

        assert 'type' in _op
        _op['type'] == 'account_witness_vote_operation'

        assert 'value' in _op
        _value = _op['value']

        assert 'account' in _value and _value['account'] == 'alice'
        assert 'proxy' in _value and _value['proxy'] == 'initminer'
