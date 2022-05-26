from test_tools import Account, logger, World, Asset
from .utilities import check_ask, check_sell_price

def test_order(wallet):
    wallet.api.create_account('initminer', 'alice', '{}')

    wallet.api.transfer('initminer', 'alice', Asset.Test(77), 'lime')

    wallet.api.transfer_to_vesting('initminer', 'alice', Asset.Test(500))

    response = wallet.api.create_order('alice', 666, Asset.Test(7), Asset.Tbd(1), False, 3600 )

    _ops = response['operations']
    assert _ops[0][1]['amount_to_sell'] == Asset.Test(7)
    assert _ops[0][1]['min_to_receive'] == Asset.Tbd(1)

    response = wallet.api.create_order('alice', 667, Asset.Test(8), Asset.Tbd(2), False, 3600 )

    assert 'operations' in response

    response = wallet.api.get_order_book(5)

    _asks = response['asks']
    assert len(_asks) == 2
    check_ask( _asks[0], Asset.Test(7), Asset.Tbd(1) )
    check_ask( _asks[1], Asset.Test(8), Asset.Tbd(2) )

    _result = wallet.api.get_open_orders('alice')
    assert len(_result) == 2
    check_sell_price( _result[0], Asset.Test(7), Asset.Tbd(1) )
    check_sell_price( _result[1], Asset.Test(8), Asset.Tbd(2) )

    response = wallet.api.cancel_order('alice', 667)

    assert len(response['operations']) == 1

    response = wallet.api.get_order_book(5)

    _asks = response['asks']
    assert len(_asks) == 1

    _result = wallet.api.get_open_orders('alice')
    assert len(_result) == 1
    check_sell_price( _result[0], Asset.Test(7), Asset.Tbd(1) )
