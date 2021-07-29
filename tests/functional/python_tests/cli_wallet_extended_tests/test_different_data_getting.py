from test_tools import Account, logger, World, Asset

def test_getters(world):
    init_node = world.create_init_node()
    init_node.run()

    wallet = init_node.attach_wallet()

    response = wallet.api.create_account('initminer', 'alice', '{}')

    transaction_id = response['result']['transaction_id']

    response = wallet.api.transfer_to_vesting('initminer', 'alice', Asset.Test(500))

    block_number = response['result']['ref_block_num'] + 1

    response = wallet.api.get_block(block_number)

    _trx = response['result']['transactions'][0]
    _ops = _trx['operations']

    _value = _ops[0][1]
    assert _value['amount'] == Asset.Test(500)

    response = wallet.api.get_encrypted_memo('alice', 'initminer', '#this is memo')

    _encrypted = response['result']

    response = wallet.api.decrypt_memo(_encrypted)

    assert response['result'] == 'this is memo'

    response = wallet.api.get_feed_history()

    _current_median_history = response['result']['current_median_history']
    assert _current_median_history['base'] == Asset.Tbd(0.001)
    assert _current_median_history['quote'] == Asset.Test(0.001)

    with wallet.in_single_transaction() as transaction:
        wallet.api.create_account('initminer', 'bob', '{}')
        wallet.api.create_account('initminer', 'carol', '{}')

    _response = transaction.get_response()

    block_number = _response['result']['ref_block_num'] + 1

    logger.info('Waiting...')
    init_node.wait_number_of_blocks(22)

    response = wallet.api.get_ops_in_block( block_number, False )
    _result = response['result']

    assert len(_result) == 5
    trx = _result[4]

    assert 'vesting_shares' in trx['op'][1]
    assert trx['op'][1]['vesting_shares'] != '0.000000 VESTS'

    response = wallet.api.get_prototype_operation( 'transfer_operation' )

    _value = response['result'][1]
    assert _value['amount'] == Asset.Test(0)

    response = wallet.api.get_transaction(transaction_id)

    _ops = response['result']['operations']
    assert _ops[0][0] == 'account_create'

    assert 'fee' in _ops[0][1]
    assert _ops[0][1]['fee'] == Asset.Test(0)
