from test_tools import Account, logger, World

def test_delegate():
    with World() as world:
        init_node = world.create_init_node()
        init_node.run()

        wallet = init_node.attach_wallet()

        #**************************************************************
        logger.info('create_account...')
        response = wallet.api.create_account('initminer', 'alice', '{}')
        logger.info(response)

        #**************************************************************
        logger.info('transfer...')
        response = wallet.api.transfer('initminer', 'alice', '200.000 TESTS', 'avocado')
        logger.info(response)

        #**************************************************************
        logger.info('transfer...')
        response = wallet.api.transfer('initminer', 'alice', '100.000 TBD', 'banana')
        logger.info(response)

        #**************************************************************
        logger.info('transfer_to_vesting...')
        response = wallet.api.transfer_to_vesting('initminer', 'alice', '50.000 TESTS')
        logger.info(response)

        #**************************************************************
        logger.info('create_account...')
        response = wallet.api.create_account('alice', 'bob', '{}')
        logger.info(response)

        #**************************************************************
        logger.info('transfer...')
        response = wallet.api.transfer('alice', 'bob', '50.000 TESTS', 'lemon')
        logger.info(response)

        #**************************************************************
        logger.info('transfer_to_vesting...')
        response = wallet.api.transfer_to_vesting('alice', 'bob', '25.000 TESTS')
        logger.info(response)

        #**************************************************************
        logger.info('post_comment...')
        response = wallet.api.post_comment('alice', 'test-permlink', '', 'xyz', 'śćą', 'DEBUG    test_tools.wallet.World.InitNodeWallet0:wallet.py:462 Closed with 0 return code', '{}')
        logger.info(response)

        _result = response['result']

        _ops = _result['operations']

        assert len(_ops) == 1
        _op = _ops[0]

        assert 'type' in _op and _op['type'] == 'comment_operation'

        assert 'value' in _op
        _value = _op['value']

        assert 'author' in _value and _value['author'] == 'alice'
        assert 'title' in _value and _value['title'] == 'u015bu0107u0105'
        assert 'body' in _value and _value['body'] == 'DEBUG    test_tools.wallet.World.InitNodeWallet0:wallet.py:462 Closed with 0 return code'

        #**************************************************************
        logger.info('post_comment...')
        response = wallet.api.post_comment('alice', 'test-permlink', '', 'xyz', 'TITLE NR 2', 'BODY NR 2', '{}')
        logger.info(response)

        _result = response['result']
        _ops = _result['operations']

        assert len(_ops) == 1
        _op = _ops[0]

        assert 'type' in _op and _op['type'] == 'comment_operation'

        assert 'value' in _op
        _value = _op['value']

        assert 'author' in _value and _value['author'] == 'alice'
        assert 'title' in _value and _value['title'] == 'TITLE NR 2'
        assert 'body' in _value and _value['body'] == 'BODY NR 2'

        #**************************************************************
        logger.info('post_comment...')
        response = wallet.api.post_comment('bob', 'bob-permlink', 'alice', 'test-permlink', 'TITLE NR 3', 'BODY NR 3', '{}')
        logger.info(response)

        _result = response['result']
        _ops = _result['operations']

        assert len(_ops) == 1
        _op = _ops[0]

        assert 'type' in _op and _op['type'] == 'comment_operation'

        assert 'value' in _op
        _value = _op['value']

        assert 'author' in _value and _value['author'] == 'bob'
        assert 'parent_author' in _value and _value['parent_author'] == 'alice'
        assert 'title' in _value and _value['title'] == 'TITLE NR 3'
        assert 'body' in _value and _value['body'] == 'BODY NR 3'

        #**************************************************************
        logger.info('vote...')
        response = wallet.api.vote('bob', 'bob', 'bob-permlink', 100)
        logger.info(response)

        _result = response['result']
        _ops = _result['operations']

        assert len(_ops) == 1
        _op = _ops[0]

        assert 'type' in _op
        _op['type'] == 'vote_operation'

        assert 'value' in _op
        _value = _op['value']

        assert 'voter' in _value and _value['voter'] == 'bob'
        assert 'author' in _value and _value['author'] == 'bob'
        assert 'permlink' in _value and _value['permlink'] == 'bob-permlink'
        assert 'weight' in _value and _value['weight'] == 10000
