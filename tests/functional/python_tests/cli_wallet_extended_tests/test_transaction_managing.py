from test_tools import Account, logger, World, Asset
import dateutil.parser as dp
import datetime
from utilities import send_with_args_and_assert_result

def test_transaction(wallet):
    wallet.api.create_account('initminer', 'carol', '{}')

    with wallet.in_single_transaction(broadcast=False) as transaction:
        wallet.api.transfer_to_vesting('initminer', 'carol', Asset.Test(100))
        wallet.api.transfer('initminer', 'carol', Asset.Test(500), 'kiwi')
        wallet.api.transfer('initminer', 'carol', Asset.Tbd(50), 'orange')

    trx_response = transaction.get_response()
    _result_trx_response = trx_response['result']

    response = wallet.api.get_account('carol')

    _result = response['result']
    assert _result['balance'] == Asset.Test(0)
    assert _result['hbd_balance'] == Asset.Tbd(0)
    assert _result['vesting_shares'] == Asset.Vest(0)

    response = wallet.api.serialize_transaction(_result_trx_response)

    assert response['result'] != '00000000000000000000000000'

    wallet.api.sign_transaction(_result_trx_response)

    response = wallet.api.get_account('carol')

    _result = response['result']
    assert _result['balance'] == Asset.Test(500)
    assert _result['hbd_balance'] == Asset.Tbd(50)
    assert _result['vesting_shares'] != Asset.Vest(0)

    _time = datetime.datetime.utcnow()
    _before_seconds = (int)(_time.timestamp())
    logger.info('_time: {} seconds:{}...'.format(_time, _before_seconds))

    response = wallet.api.transfer_to_savings('initminer', 'carol', Asset.Test(0.007), 'plum')

    _expiration = response['result']['expiration']

    parsed_t = dp.parse(_expiration)
    t_in_seconds = parsed_t.timestamp()
    logger.info('_time: {} seconds:{}...'.format(_expiration, t_in_seconds))

    _val = t_in_seconds - _before_seconds
    assert _val == 30 or _val == 31

    send_with_args_and_assert_result(wallet.api.set_transaction_expiration, 678, None)

    _time = datetime.datetime.utcnow()
    _before_seconds = (int)(_time.timestamp())
    logger.info('_time: {} seconds:{}...'.format(_time, _before_seconds))

    response = wallet.api.transfer_to_savings('initminer', 'carol', Asset.Test(0.008), 'lemon')

    _expiration = response['result']['expiration']

    parsed_t = dp.parse(_expiration)
    t_in_seconds = parsed_t.timestamp()
    logger.info('_time: {} seconds:{}...'.format(_expiration, t_in_seconds))

    _val = t_in_seconds - _before_seconds
    assert _val == 678 or _val == 679
