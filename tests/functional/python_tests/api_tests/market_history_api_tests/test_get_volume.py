import pytest

import test_tools as tt
from hive_local_tools import run_for


@pytest.mark.parametrize(
    'number_of_transactions, tests_volume, tbds_volume', (
        (1, 100, 10),
        (2, 300, 60),
        (3, 600, 100),
    )
)
@run_for('testnet')
def test_check_if_get_volume_returns_correct_values(node, number_of_transactions, tests_volume, tbds_volume):
    wallet = tt.Wallet(attach_to=node)
    wallet.create_account('alice', hives=tt.Asset.Test(1000), vests=tt.Asset.Test(100))

    wallet.api.create_order('alice', 3, tt.Asset.Test(100), tt.Asset.Tbd(10), False, 3600)
    wallet.api.create_order('initminer', 2, tt.Asset.Tbd(10), tt.Asset.Test(100), False, 3600)
    if number_of_transactions in [2, 3]:
        wallet.api.create_order('alice', 4, tt.Asset.Test(200), tt.Asset.Tbd(50), False, 3600)
        wallet.api.create_order('initminer', 5, tt.Asset.Tbd(50), tt.Asset.Test(200), False, 3600)
    if number_of_transactions == 3:
        wallet.api.create_order('alice', 5, tt.Asset.Test(300), tt.Asset.Tbd(40), False, 3600)
        wallet.api.create_order('initminer', 6, tt.Asset.Tbd(40), tt.Asset.Test(300), False, 3600)

    response = node.api.market_history.get_volume()
    assert response['hive_volume'] == tt.Asset.Test(tests_volume)
    assert response['hbd_volume'] == tt.Asset.Tbd(tbds_volume)

@pytest.mark.parametrize(
    # every offer - amount_to_sell, min_to_receive
    'operations, first_order, second_order',
    (
        ('only_asks', [tt.Asset.Test(100), tt.Asset.Tbd(100)], [tt.Asset.Test(200), tt.Asset.Tbd(200)]),
        ('only_bids', [tt.Asset.Tbd(30), tt.Asset.Test(100)], [tt.Asset.Tbd(40), tt.Asset.Test(200)]),
        ('nothing', 'empty value' , 'empty value')
    )
)
@run_for('testnet')
def test_get_zero_volume(node, operations, first_order, second_order):
    if operations != 'nothing':
        wallet = tt.Wallet(attach_to=node)
        wallet.api.create_order('initminer', 0, first_order[0], first_order[1], False, 3600)
        wallet.api.create_order('initminer', 1, second_order[0], second_order[1], False, 3600)
    response = node.api.market_history.get_volume()
    assert response['hive_volume'] == tt.Asset.Test(0)
    assert response['hbd_volume'] == tt.Asset.Tbd(0)
