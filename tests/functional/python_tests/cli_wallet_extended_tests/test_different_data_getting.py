from test_tools import Account, logger, World

def test_getters():
    with World() as world:
        init_node = world.create_init_node()
        init_node.run()

        wallet = init_node.attach_wallet()

        #**************************************************************
        response = wallet.api.create_account('initminer', 'alice', '{}')
        _result = response['result']

        transaction_id = _result['transaction_id']
        #**************************************************************
        response = wallet.api.transfer_to_vesting('initminer', 'alice', '500.000 TESTS')
        _result = response['result']

        block_number = _result['ref_block_num'] + 1

        #**************************************************************
        response = wallet.api.get_block(block_number)
        _result = response['result']

        _trx = _result['transactions'][0]

        _ops = _trx['operations']
        _op = _ops[0]

        _value = _op[1]
        assert _value['amount'] == '500.000 TESTS'

        #**************************************************************
        response = wallet.api.get_encrypted_memo('alice', 'initminer', '#this is memo')
        _encrypted = response['result']

        #**************************************************************
        response = wallet.api.decrypt_memo(_encrypted)
        assert response['result'] == 'this is memo'

        #**************************************************************
        response = wallet.api.get_feed_history()
        _result = response['result']

        _current_median_history = _result['current_median_history']

        assert _current_median_history['base'] == '0.001 TBD'
        assert _current_median_history['quote'] == '0.001 TESTS'

        #**************************************************************
        with wallet.in_single_transaction() as transaction:
            wallet.api.create_account('initminer', 'bob', '{}')
            wallet.api.create_account('initminer', 'carol', '{}')

        _response = transaction.get_response()

        _result = _response['result']

        block_number = _result['ref_block_num'] + 1

        #**************************************************************
        logger.info('Waiting...')
        init_node.wait_number_of_blocks(22)

        #**************************************************************
        response = wallet.api.get_ops_in_block( block_number, False )
        _result = response['result']

        assert len(_result) == 5
        trx = _result[4]

        _op = trx['op']
        _value = _op[1]

        assert 'vesting_shares' in _value
        assert _value['vesting_shares'] != '0.000000 VESTS'

        #**************************************************************
        response = wallet.api.get_prototype_operation( 'transfer_operation' )
        _result = response['result']

        _value = _result[1]
        assert _value['amount'] == '0.000 TESTS'

        #**************************************************************
        response = wallet.api.get_transaction(transaction_id)
        _result = response['result']

        _ops = _result['operations']
        _op = _ops[0]

        assert _op[0] == 'account_create'

        _value = _op[1]

        assert 'fee' in _value
        assert _value['fee'] == '0.000 TESTS'
