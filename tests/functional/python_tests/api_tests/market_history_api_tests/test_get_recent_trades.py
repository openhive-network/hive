import pytest

import test_tools as tt
from hive_local_tools import run_for

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
def test_recent_trades_output_parameters(node, limit_orders):
    wallet = tt.Wallet(attach_to=node)
    wallet.create_account('alice', hives=tt.Asset.Test(600), vests=tt.Asset.Test(100))
    for number in range(len(limit_orders)):
        order = 'order_'+ str(number)
        wallet.api.create_order('alice', number, tt.Asset.Test(limit_orders[order]['tests']),
                                tt.Asset.Tbd(limit_orders[order]['tbds']), False, 3600)
        wallet.api.create_order('initminer', number, tt.Asset.Tbd(limit_orders[order]['tbds']),
                                tt.Asset.Test(limit_orders[order]['tests']), False, 3600)
    response = node.api.market_history.get_recent_trades()['trades']
    assert len(response) == 3

    # trades appear in response from last to first
    for x in range(len(response)):
        assert tt.Asset.Tbd(limit_orders['order_'+str(x)]['tbds']) == response[len(response)-x-1]['current_pays']
        assert tt.Asset.Test(limit_orders['order_' + str(x)]['tests']) == response[len(response)-x-1]['open_pays']


@pytest.mark.parametrize(
    'limit', (1,2)
)
@run_for('testnet')
def test_limit(node, limit):
    wallet = tt.Wallet(attach_to=node)
    wallet.create_account('alice', hives=tt.Asset.Test(600), vests=tt.Asset.Test(300))

    wallet.api.create_order('alice', 0, tt.Asset.Test(100), tt.Asset.Tbd(30), False, 3600)
    wallet.api.create_order('initminer', 0, tt.Asset.Tbd(30), tt.Asset.Test(100), False, 3600)

    wallet.api.create_order('alice', 1, tt.Asset.Test(200), tt.Asset.Tbd(40), False, 3600)
    wallet.api.create_order('initminer', 1, tt.Asset.Tbd(40), tt.Asset.Test(200), False, 3600)
    if limit == 2:
        wallet.api.create_order('alice', 2, tt.Asset.Test(300), tt.Asset.Tbd(50), False, 3600)
        wallet.api.create_order('initminer', 2, tt.Asset.Tbd(50), tt.Asset.Test(300), False, 3600)
    response = node.api.market_history.get_recent_trades(limit=limit)['trades']
    assert len(response) == limit
