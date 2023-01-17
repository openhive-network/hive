import pytest

import test_tools as tt
from hive_local_tools import run_for

@pytest.mark.parametrize(
    'ask_hbd_amount, ask_hive_amount, bid_hbd_amount, bid_hive_amount', (
        (50, 300, 30, 200),
        (25, 250, 20, 300)
    )
)
@run_for('testnet')
def test_get_order_book_with_different_values(node, ask_hbd_amount, ask_hive_amount, bid_hbd_amount, bid_hive_amount):
    wallet = tt.Wallet(attach_to=node)
    wallet.create_account('alice', hives=tt.Asset.Test(500), vests=tt.Asset.Test(100))

    wallet.api.create_order('alice', 0, tt.Asset.Test(ask_hive_amount), tt.Asset.Tbd(ask_hbd_amount), False, 3600)
    wallet.api.create_order('initminer', 1, tt.Asset.Tbd(bid_hbd_amount), tt.Asset.Test(bid_hive_amount), False, 3600)

    response = node.api.market_history.get_order_book()
    assert len(response['asks']) == 1
    assert len(response['bids']) == 1

    assert response['asks'][0]['order_price']['base'] == tt.Asset.Test(ask_hive_amount)
    assert response['asks'][0]['order_price']['quote'] == tt.Asset.Tbd(ask_hbd_amount)
    assert float(response['asks'][0]['real_price']) == ask_hbd_amount/ask_hive_amount

    assert response['bids'][0]['order_price']['base'] == tt.Asset.Tbd(bid_hbd_amount)
    assert response['bids'][0]['order_price']['quote'] == tt.Asset.Test(bid_hive_amount)
    assert float(response['bids'][0]['real_price']) == bid_hbd_amount/bid_hive_amount


@run_for('testnet')
def test_get_order_book_after_successful_transaction_finishing_all_orders(node):
    wallet = tt.Wallet(attach_to=node)
    wallet.create_account('alice', hives=tt.Asset.Test(500), vests=tt.Asset.Test(100))

    wallet.api.create_order('alice', 0, tt.Asset.Test(300), tt.Asset.Tbd(50), False, 3600)
    wallet.api.create_order('initminer', 1, tt.Asset.Tbd(50), tt.Asset.Test(300), False, 3600)

    response = node.api.market_history.get_order_book()
    assert len(response['bids']) == 0
    assert len(response['asks']) == 0


@run_for('testnet')
def test_exceed_limit_parameter(node):
    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.market_history.get_order_book(limit=501)


@pytest.mark.parametrize(
    'limit', (1,2)
)
@run_for('testnet')
def test_limit(node, limit):
    wallet = tt.Wallet(attach_to=node)
    wallet.create_account('alice', hives=tt.Asset.Test(600), vests=tt.Asset.Test(300))

    wallet.api.create_order('alice', 0, tt.Asset.Test(100), tt.Asset.Tbd(100), False, 3600)
    wallet.api.create_order('initminer', 0, tt.Asset.Tbd(30), tt.Asset.Test(100), False, 3600)

    wallet.api.create_order('alice', 1, tt.Asset.Test(200), tt.Asset.Tbd(200), False, 3600)
    wallet.api.create_order('initminer', 1, tt.Asset.Tbd(40), tt.Asset.Test(200), False, 3600)
    if limit==2:
        wallet.api.create_order('alice', 2, tt.Asset.Test(300), tt.Asset.Tbd(300), False, 3600)
        wallet.api.create_order('initminer', 2, tt.Asset.Tbd(50), tt.Asset.Test(300), False, 3600)
    response = node.api.market_history.get_order_book(limit=limit)
    assert len(response['bids']) == limit
    assert len(response['asks']) == limit
