from test_tools import Account, logger, World

def test_witness():
    with World() as world:
        init_node = world.create_init_node()
        init_node.run()

        wallet = init_node.attach_wallet()

        #**************************************************************
        response = wallet.api.create_account('initminer', 'alice', '{}')
        assert 'result' in response

        #**************************************************************
        response = wallet.api.transfer_to_vesting('initminer', 'alice', '500.000 TESTS')
        assert 'result' in response

        #**************************************************************
        response = wallet.api.get_active_witnesses()
        _result = response['result']

        assert len(_result) > 1
        assert _result[0] == 'initminer'
        assert _result[1] == ''

        #**************************************************************
        response = wallet.api.list_witnesses('a', 4)
        _result = response['result']

        assert len(_result) == 1
        assert _result[0] == 'initminer'
        #**************************************************************
        response = wallet.api.update_witness('alice', 'http:\\url.html', 'TST6LLegbAgLAy28EHrffBVuANFWcFgmqRMW13wBmTExqFE9SCkg4', { 'account_creation_fee':'2789.030 TESTS', 'maximum_block_size' : 131072, 'hbd_interest_rate' : 1000 } )
        assert 'result' in response

        #**************************************************************
        response = wallet.api.get_active_witnesses()
        _result = response['result']

        assert len(_result) > 1
        assert _result[0] == 'initminer'
        assert _result[1] == ''

        #**************************************************************
        response = wallet.api.list_witnesses('a', 4)
        _result = response['result']

        assert len(_result) == 2
        assert _result[0] == 'alice'
        assert _result[1] == 'initminer'
        #**************************************************************
        response = wallet.api.get_witness('alice')
        _result = response['result']

        assert _result['owner'] == 'alice'

        _props = _result['props']
        assert _props['account_creation_fee'] == '2789.030 TESTS'

        #**************************************************************
        response = wallet.api.publish_feed('alice', {"base":"1.167 TBD", "quote":"1.111 TESTS"})
        _result = response['result']

        _ops = _result['operations']
        _op = _ops[0]

        assert _op['type'] == 'feed_publish_operation'

        _value = _op['value']
        _exchange_rate = _value['exchange_rate']

        assert _exchange_rate['base'] == '1.167 TBD'
        assert _exchange_rate['quote'] == '1.111 TESTS'

        #**************************************************************
        response = wallet.api.vote_for_witness('initminer', 'alice', True)
        _result = response['result']

        _ops = _result['operations']
        _op = _ops[0]

        assert _op['type'] == 'account_witness_vote_operation'

        _value = _op['value']
        assert _value['account'] == 'initminer'
        assert _value['witness'] == 'alice'
        assert _value['approve'] == True

        #**************************************************************
        response = wallet.api.set_voting_proxy('alice', 'initminer')
        _result = response['result']

        _ops = _result['operations']
        _op = _ops[0]

        assert _op['type'] == 'account_witness_proxy_operation'

        _value = _op['value']
        assert _value['account'] == 'alice'
        assert _value['proxy'] == 'initminer'