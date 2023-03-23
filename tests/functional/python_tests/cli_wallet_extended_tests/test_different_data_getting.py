import test_tools as tt
from hive_local_tools import run_for

@run_for('testnet', enable_plugins=['account_history_api'])
def test_getters(node, wallet):
    response = wallet.api.create_account('initminer', 'alice', '{}')

    transaction_id = response['transaction_id']

    response = wallet.api.transfer_to_vesting('initminer', 'alice', tt.Asset.Test(500))

    block_number = response['ref_block_num'] + 1

    response = wallet.api.get_block(block_number)

    _trx = response['transactions'][0]
    _ops = _trx['operations']

    _value = _ops[0][1]
    assert _value['amount'] == tt.Asset.Test(500)

    _encrypted = wallet.api.get_encrypted_memo('alice', 'initminer', '#this is memo')

    assert wallet.api.decrypt_memo(_encrypted) == 'this is memo'

    response = wallet.api.get_feed_history()

    _current_median_history = response['current_median_history']
    assert _current_median_history['base'] == tt.Asset.Tbd(0.001)
    assert _current_median_history['quote'] == tt.Asset.Test(0.001)

    with wallet.in_single_transaction() as transaction:
        wallet.api.create_account('initminer', 'bob', '{}')
        wallet.api.create_account('initminer', 'carol', '{}')

    _response = transaction.get_response()

    block_number = _response['ref_block_num'] + 1

    tt.logger.info('Waiting...')
    node.wait_for_irreversible_block()

    _result = wallet.api.get_ops_in_block( block_number, False )

    assert len(_result) == 5
    trx = _result[4]

    assert 'vesting_shares' in trx['op'][1]
    assert trx['op'][1]['vesting_shares'] != tt.Asset.Vest(0)

    response = wallet.api.get_prototype_operation( 'transfer_operation' )

    _value = response[1]
    assert _value['amount'] == tt.Asset.Test(0)

    response = wallet.api.get_transaction(transaction_id)

    _ops = response['operations']
    assert _ops[0][0] == 'account_create'

    assert 'fee' in _ops[0][1]
    assert _ops[0][1]['fee'] == tt.Asset.Test(0)
