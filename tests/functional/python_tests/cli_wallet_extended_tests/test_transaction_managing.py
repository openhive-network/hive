from test_tools import Account, logger, World
import dateutil.parser as dp
import datetime

def test_transaction(world):
    init_node = world.create_init_node()
    init_node.run()

    wallet = init_node.attach_wallet()

    #**************************************************************
    response = wallet.api.create_account('initminer', 'carol', '{}')
    assert 'result' in response

    #**************************************************************
    with wallet.in_single_transaction(broadcast=False) as transaction:
        wallet.api.transfer_to_vesting('initminer', 'carol', '100.000 TESTS')
        wallet.api.transfer('initminer', 'carol', '500.000 TESTS', 'kiwi')
        wallet.api.transfer('initminer', 'carol', '50.000 TBD', 'orange')

    trx_response = transaction.get_response()

    _result_trx_response = trx_response['result']

    #**************************************************************
    response = wallet.api.get_account('carol')
    _result = response['result']

    assert _result['balance'] == '0.000 TESTS'
    assert _result['hbd_balance'] == '0.000 TBD'
    assert _result['vesting_shares'] == '0.000000 VESTS'

    #**************************************************************
    response = wallet.api.serialize_transaction(_result_trx_response)
    assert response['result'] != '00000000000000000000000000'

    #**************************************************************
    response = wallet.api.sign_transaction(_result_trx_response)
    assert 'result' in response

    #**************************************************************
    response = wallet.api.get_account('carol')
    _result = response['result']

    assert _result['balance'] == '500.000 TESTS'
    assert _result['hbd_balance'] == '50.000 TBD'
    assert _result['vesting_shares'] != '0.000000 VESTS'

    #**************************************************************
    _time = datetime.datetime.utcnow()
    _before_seconds = (int)(_time.timestamp())
    logger.info('_time: {} seconds:{}...'.format(_time, _before_seconds))

    response = wallet.api.transfer_to_savings('initminer', 'carol', '0.007 TESTS', 'plum')

    _expiration = response['result']['expiration']

    parsed_t = dp.parse(_expiration)
    t_in_seconds = parsed_t.timestamp()
    logger.info('_time: {} seconds:{}...'.format(_expiration, t_in_seconds))

    _val = t_in_seconds - _before_seconds
    assert _val == 30 or _val == 31
    #**************************************************************
    response = wallet.api.set_transaction_expiration(678)
    assert response['result'] == None

    #**************************************************************
    _time = datetime.datetime.utcnow()
    _before_seconds = (int)(_time.timestamp())
    logger.info('_time: {} seconds:{}...'.format(_time, _before_seconds))

    response = wallet.api.transfer_to_savings('initminer', 'carol', '0.008 TESTS', 'lemon')

    _expiration = response['result']['expiration']

    parsed_t = dp.parse(_expiration)
    t_in_seconds = parsed_t.timestamp()
    logger.info('_time: {} seconds:{}...'.format(_expiration, t_in_seconds))

    _val = t_in_seconds - _before_seconds
    assert _val == 678 or _val == 679
