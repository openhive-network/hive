from test_tools import Account, logger, World

def test_getters():
    with World() as world:
        init_node = world.create_init_node()
        init_node.config.plugin.append('database_api')
        init_node.config.plugin.append('network_broadcast_api')
        init_node.config.plugin.append('account_history_rocksdb')
        init_node.config.plugin.append('account_history')
        init_node.config.plugin.append('account_history_api')
        init_node.config.plugin.append('block_api')
        init_node.run()

        wallet = init_node.attach_wallet()

        #**************************************************************
        logger.info('create_account...')
        response = wallet.api.create_account('initminer', 'alice', '{}')
        logger.info(response)

        assert 'result' in response
        _result = response['result']

        assert 'transaction_id' in _result
        transaction_id = _result['transaction_id']
        #**************************************************************
        logger.info('transfer_to_vesting...')
        response = wallet.api.transfer_to_vesting('initminer', 'alice', '500.000 TESTS')
        logger.info(response)

        assert 'result' in response
        _result = response['result']

        assert 'ref_block_num' in _result
        block_number = _result['ref_block_num'] + 1

        #**************************************************************
        logger.info('get_block...')
        response = wallet.api.get_block(block_number)
        logger.info(response)

        assert 'result' in response
        _result = response['result']

        assert 'transactions' in _result
        assert len(_result['transactions']) == 1
        _trx = _result['transactions'][0]

        assert 'operations' in _trx
        _ops = _trx['operations']

        assert len(_ops) == 1

        _op = _ops[0]
        assert 'value' in _op
        _value = _op['value']

        assert 'amount' in _value
        assert _value['amount'] == '500.000 TESTS'

        #**************************************************************
        logger.info('get_encrypted_memo...')
        response = wallet.api.get_encrypted_memo('alice', 'initminer', '#this is memo')
        logger.info(response)

        assert 'result' in response
        response['result'] == '#FZNN15uqMGdU1vzeMiHyzo6p8hT4V4WHGLNbTUGprhQFMVDYD4jc35zgSYf3BDL6vpbvNkBo831ttojfstt7bH58PC1etd9qbUHtoA6ZRqqpnzAsPg4rubGd2ANGyHvce'

        #**************************************************************
        logger.info('decrypt_memo...')
        response = wallet.api.decrypt_memo('#FZNN15uqMGdU1vzeMiHyzo6p8hT4V4WHGLNbTUGprhQFMVDYD4jc35zgSYf3BDL6vpbvNkBo831ttojfstt7bH58PC1etd9qbUHtoA6ZRqqpnzAsPg4rubGd2ANGyHvce')
        logger.info(response)

        assert 'result' in response
        response['result'] == 'this is memo'

        #**************************************************************
        logger.info('get_feed_history...')
        response = wallet.api.get_feed_history()
        logger.info(response)

        assert 'result' in response
        _result = response['result']

        assert 'current_median_history' in _result
        _current_median_history = _result['current_median_history']

        assert 'base' in _current_median_history
        _current_median_history['base'] == '0.001 TBD'

        assert 'quote' in _current_median_history
        _current_median_history['quote'] == '0.001 TESTS'

        #**************************************************************
        logger.info('create_account x2...')
        with wallet.in_single_transaction():
          wallet.api.create_account('initminer', 'bob', '{}')
          wallet.api.create_account('initminer', 'carol', '{}')

        _response = transaction.get_response()

        logger.info(_response)

        assert 'result' in _response
        _result = _response['result']

        assert 'ref_block_num' in _result
        block_number = _result['ref_block_num'] + 1

        #**************************************************************
        logger.info('Waiting...')
        init_node.wait_number_of_blocks(22)

        #**************************************************************
        logger.info('get_ops_in_block...')
        response = wallet.api.get_ops_in_block( block_number, False )
        logger.info(response)

        assert 'result' in response
        _result = response['result']

        assert len(_result) == 5
        trx = _result[4]

        assert 'op' in trx
        _op = trx['op']

        assert 'value' in _op
        _value = _op['value']

        assert 'vesting_shares' in _value
        assert _value['vesting_shares'] != '0.000000 VESTS'

        #**************************************************************
        logger.info('get_prototype_operation...')
        response = wallet.api.get_prototype_operation( 'transfer_operation' )
        logger.info(response)

        assert 'result' in response
        _result = response['result']

        assert 'value' in _result
        _value = _result['value']

        assert 'amount' in _value
        assert _value['amount'] == '0.000 TESTS'

        #**************************************************************
        logger.info('get_transaction...')
        response = wallet.api.get_transaction(transaction_id)
        logger.info(response)

        assert 'result' in response
        _result = response['result']

        assert 'operations' in _result
        _ops = _result['operations']

        assert len(_ops) == 1
        _op = _ops[0]

        assert 'type' in _op
        _op['type'] == 'account_create_operation'

        assert 'value' in _op
        _value = _op['value']

        assert 'fee' in _value
        assert _value['fee'] == '0.000 TESTS'

