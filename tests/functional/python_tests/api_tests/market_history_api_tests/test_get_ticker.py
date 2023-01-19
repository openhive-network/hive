import pytest

import test_tools as tt
from hive_local_tools import run_for


HIVE_AND_HBD_AMOUNTS = (
    ([153, 241], [1095, 1331]),
    ([40, 50], [100, 200]),
    ([1, 15], [55, 88]),
)
@pytest.mark.parametrize(
    'limit_orders', (
            # lowest price first, last highest
            (
                {
                    'order_0':{'tests': 100, 'tbds': 10},
                    'order_1': {'tests': 200, 'tbds': 40} ,
                    'order_2': {'tests': 300, 'tbds': 40}
                 }
            ),
            # highest price first, last lowest
            (
                {
                    'order_0':{'tests': 200, 'tbds': 40},
                    'order_1': {'tests': 300, 'tbds': 40} ,
                    'order_2': {'tests': 100, 'tbds': 10}
                 }
            ),
            # same price in three successful transactions
            (
                {
                    'order_0':{'tests': 100, 'tbds': 10},
                    'order_1': {'tests': 100, 'tbds': 10} ,
                    'order_2': {'tests': 100, 'tbds': 10}
                 }
            ),
    )
)
@run_for('testnet')
def test_ticker_output_parameters(node, limit_orders):
    wallet = tt.Wallet(attach_to=node)
    wallet.create_account('alice', hives=tt.Asset.Test(600), vests=tt.Asset.Test(100))
    hive_volume = 0
    hbd_volume = 0
    for number in range(3):
        order = 'order_'+ str(number)
        wallet.api.create_order('alice', number, tt.Asset.Test(limit_orders[order]['tests']),
                                tt.Asset.Tbd(limit_orders[order]['tbds']), False, 3600)
        wallet.api.create_order('initminer', number, tt.Asset.Tbd(limit_orders[order]['tbds']),
                                tt.Asset.Test(limit_orders[order]['tests']), False, 3600)
        hive_volume += limit_orders[order]['tests']
        hbd_volume += limit_orders[order]['tbds']

    response = node.api.market_history.get_ticker()

    latest = limit_orders['order_2']['tbds'] / limit_orders['order_2']['tests']
    open = limit_orders['order_0']['tbds'] / limit_orders['order_0']['tests']

    assert float(response['percent_change']) == (latest-open)/open * 100
    assert response['hive_volume'] == tt.Asset.Test(hive_volume)
    assert response['hbd_volume'] == tt.Asset.Tbd(hbd_volume)
    assert float(response['latest']) == latest


@pytest.mark.parametrize(
    'hive_amount, hbd_amount', HIVE_AND_HBD_AMOUNTS
)
@run_for('testnet')
def test_lowest_ask_in_ticker(node, hive_amount, hbd_amount):
    wallet = tt.Wallet(attach_to=node)

    wallet.api.create_order('initminer', 0, tt.Asset.Test(hive_amount[0]), tt.Asset.Tbd(hbd_amount[0]), False, 3600)
    wallet.api.create_order('initminer', 1, tt.Asset.Test(hive_amount[1]), tt.Asset.Tbd(hbd_amount[1]), False, 3600)

    response = node.api.market_history.get_ticker()
    assert float(response['lowest_ask']) == min(hbd_amount[0]/hive_amount[0], hbd_amount[1]/hive_amount[1])


@pytest.mark.parametrize(
    'hive_amount, hbd_amount', HIVE_AND_HBD_AMOUNTS
)
@run_for('testnet')
def test_highest_bid_in_ticker(node, hive_amount, hbd_amount):
    wallet = tt.Wallet(attach_to=node)

    wallet.api.create_order('initminer', 0, tt.Asset.Tbd(hbd_amount[0]), tt.Asset.Test(hive_amount[0]), False, 3600)
    wallet.api.create_order('initminer', 1, tt.Asset.Tbd(hbd_amount[1]), tt.Asset.Test(hive_amount[1]), False, 3600)
    response = node.api.market_history.get_ticker()
    assert float(response['highest_bid']) == max(hbd_amount[0]/hive_amount[0], hbd_amount[1]/hive_amount[1])
