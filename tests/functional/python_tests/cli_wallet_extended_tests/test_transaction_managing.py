import datetime

import pytest

import test_tools as tt

from ..local_tools import parse_datetime


def test_transaction(wallet):
    wallet.api.create_account('initminer', 'carol', '{}')

    with wallet.in_single_transaction(broadcast=False) as transaction:
        wallet.api.transfer_to_vesting('initminer', 'carol', tt.Asset.Test(100))
        wallet.api.transfer('initminer', 'carol', tt.Asset.Test(500), 'kiwi')
        wallet.api.transfer('initminer', 'carol', tt.Asset.Tbd(50), 'orange')

    _result_trx_response = transaction.get_response()

    _result = wallet.api.get_account('carol')
    assert _result['balance'] == tt.Asset.Test(0)
    assert _result['hbd_balance'] == tt.Asset.Tbd(0)
    assert _result['vesting_shares'] == tt.Asset.Vest(0)

    assert wallet.api.serialize_transaction(_result_trx_response) != '00000000000000000000000000'

    wallet.api.sign_transaction(_result_trx_response)

    _result = wallet.api.get_account('carol')
    assert _result['balance'] == tt.Asset.Test(500)
    assert _result['hbd_balance'] == tt.Asset.Tbd(50)
    assert _result['vesting_shares'] != tt.Asset.Vest(0)

    _time = datetime.datetime.utcnow()
    _before_seconds = (int)(_time.timestamp())
    tt.logger.info('_time: {} seconds:{}...'.format(_time, _before_seconds))

    response = wallet.api.transfer_to_savings('initminer', 'carol', tt.Asset.Test(0.007), 'plum')

    _expiration = response['expiration']

    parsed_t = parse_datetime(_expiration)
    t_in_seconds = parsed_t.timestamp()
    tt.logger.info('_time: {} seconds:{}...'.format(_expiration, t_in_seconds))

    _val = t_in_seconds - _before_seconds
    assert _val == 30 or _val == 31

    assert wallet.api.set_transaction_expiration(678) is None

    _time = datetime.datetime.utcnow()
    _before_seconds = (int)(_time.timestamp())
    tt.logger.info('_time: {} seconds:{}...'.format(_time, _before_seconds))

    response = wallet.api.transfer_to_savings('initminer', 'carol', tt.Asset.Test(0.008), 'lemon')

    _expiration = response['expiration']

    parsed_t = parse_datetime(_expiration)
    t_in_seconds = parsed_t.timestamp()
    tt.logger.info('_time: {} seconds:{}...'.format(_expiration, t_in_seconds))

    _val = t_in_seconds - _before_seconds
    assert _val == 678 or _val == 679
