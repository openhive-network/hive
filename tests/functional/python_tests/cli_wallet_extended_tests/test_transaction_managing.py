import datetime

import pytest

from test_tools import Asset, logger


def test_transaction(wallet):
    wallet.api.create_account('initminer', 'carol', '{}')

    with wallet.in_single_transaction(broadcast=False) as transaction:
        wallet.api.transfer_to_vesting('initminer', 'carol', Asset.Test(100))
        wallet.api.transfer('initminer', 'carol', Asset.Test(500), 'kiwi')
        wallet.api.transfer('initminer', 'carol', Asset.Tbd(50), 'orange')

    _result_trx_response = transaction.get_response()

    _result = wallet.api.get_account('carol')
    assert _result['balance'] == Asset.Test(0)
    assert _result['hbd_balance'] == Asset.Tbd(0)
    assert _result['vesting_shares'] == Asset.Vest(0)

    assert wallet.api.serialize_transaction(_result_trx_response) != '00000000000000000000000000'

    wallet.api.sign_transaction(_result_trx_response)

    _result = wallet.api.get_account('carol')
    assert _result['balance'] == Asset.Test(500)
    assert _result['hbd_balance'] == Asset.Tbd(50)
    assert _result['vesting_shares'] != Asset.Vest(0)

    _time = datetime.datetime.utcnow()
    _before_seconds = (int)(_time.timestamp())
    logger.info('_time: {} seconds:{}...'.format(_time, _before_seconds))

    response = wallet.api.transfer_to_savings('initminer', 'carol', Asset.Test(0.007), 'plum')

    _expiration = response['expiration']

    parsed_t = parse_datetime(_expiration)
    t_in_seconds = parsed_t.timestamp()
    logger.info('_time: {} seconds:{}...'.format(_expiration, t_in_seconds))

    _val = t_in_seconds - _before_seconds
    assert _val == 30 or _val == 31

    assert wallet.api.set_transaction_expiration(678) is None

    _time = datetime.datetime.utcnow()
    _before_seconds = (int)(_time.timestamp())
    logger.info('_time: {} seconds:{}...'.format(_time, _before_seconds))

    response = wallet.api.transfer_to_savings('initminer', 'carol', Asset.Test(0.008), 'lemon')

    _expiration = response['expiration']

    parsed_t = parse_datetime(_expiration)
    t_in_seconds = parsed_t.timestamp()
    logger.info('_time: {} seconds:{}...'.format(_expiration, t_in_seconds))

    _val = t_in_seconds - _before_seconds
    assert _val == 678 or _val == 679


@pytest.mark.parametrize(
    "way_of_broadcasting",
    [
        "node.api.condenser.broadcast_transaction(transaction)",
        "node.api.network_broadcast.broadcast_transaction(trx=transaction)",
        "node.api.wallet_bridge.broadcast_transaction(transaction)",
    ]
)
def test_broadcasting_manually_signed_transaction(node, wallet, way_of_broadcasting):
    transaction = wallet.api.create_account('initminer', 'alice', '{}', broadcast=False)
    transaction = wallet.api.sign_transaction(transaction, broadcast=False)

    eval(way_of_broadcasting)
    assert 'alice' in wallet.list_accounts()


def parse_datetime(datetime_: str) -> datetime.datetime:
    return datetime.datetime.strptime(datetime_, '%Y-%m-%dT%H:%M:%S')
