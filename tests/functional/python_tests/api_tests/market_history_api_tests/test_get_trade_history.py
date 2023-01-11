import pytest

import test_tools as tt
from hive_local_tools import run_for


@pytest.mark.skip(reason='https://gitlab.syncad.com/hive/hive/-/issues/449')
@run_for('testnet')
def test_get_trade_history_with_start_date_after_end(node):
    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.market_history.get_trade_history(start=tt.Time.from_now(weeks=10), end=tt.Time.from_now(weeks=-1))

@run_for('testnet')
def test_exceed_time_range(node):
    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.market_history.get_trade_history(start=tt.Time.from_now(years=-100), end=tt.Time.from_now(years=100))

@pytest.mark.parametrize(
    'tests_amount, tbds_amount',
    (
        (100, 10),
        (10, 100),
        (85, 30),
    )
)
@run_for('testnet')
def test_trade_history_with_different_values(node, tests_amount, tbds_amount):
    wallet = tt.Wallet(attach_to=node)
    wallet.create_account('alice', hives=tt.Asset.Test(100), vests=tt.Asset.Test(100))

    wallet.api.create_order('alice', 0, tt.Asset.Test(tests_amount), tt.Asset.Tbd(tbds_amount), False, 3600)
    wallet.api.create_order('initminer', 1, tt.Asset.Tbd(tbds_amount), tt.Asset.Test(tests_amount), False, 3600)


    response = node.api.market_history.get_trade_history(start=tt.Time.from_now(weeks=-1), end=tt.Time.from_now(weeks=1))['trades']

    assert len(response) == 1
    assert response[0]['current_pays'] == tt.Asset.Tbd(tbds_amount)
    assert response[0]['open_pays'] == tt.Asset.Hive(tests_amount)


@run_for('testnet')
def test_get_empty_trade_history(node):
    wallet = tt.Wallet(attach_to=node)
    wallet.create_account('alice', hives=tt.Asset.Test(100), vests=tt.Asset.Test(100))

    wallet.api.create_order('alice', 3, tt.Asset.Test(100), tt.Asset.Tbd(10), False, 3600)
    wallet.api.create_order('initminer', 2, tt.Asset.Tbd(10), tt.Asset.Test(100), False, 3600)

    # wait 6 blocks - more than 15 seconds
    node.wait_number_of_blocks(6)
    # ask for history from last 15 seconds
    response = node.api.market_history.get_trade_history(start=tt.Time.from_now(seconds=-15), end=tt.Time.from_now(weeks=1),
                                                         limit=10)['trades']
    assert len(response) == 0

@pytest.mark.parametrize(
    'limit', (1, 2)
)
@run_for('testnet')
def test_trade_history_limit(node, limit):
    wallet = tt.Wallet(attach_to=node)
    wallet.create_account('alice', hives=tt.Asset.Test(500), vests=tt.Asset.Test(100))

    wallet.api.create_order('alice', 0, tt.Asset.Test(300), tt.Asset.Tbd(30), False, 3600)
    wallet.api.create_order('initminer', 1, tt.Asset.Tbd(30), tt.Asset.Test(300), False, 3600)

    wallet.api.create_order('alice', 2, tt.Asset.Test(100), tt.Asset.Tbd(10), False, 3600)
    wallet.api.create_order('initminer', 3, tt.Asset.Tbd(10), tt.Asset.Test(100), False, 3600)

    if limit==2:
        wallet.api.create_order('alice', 4, tt.Asset.Test(50), tt.Asset.Tbd(5), False, 3600)
        wallet.api.create_order('initminer', 5, tt.Asset.Tbd(5), tt.Asset.Test(50), False, 3600)
    response = \
    node.api.market_history.get_trade_history(start=tt.Time.from_now(weeks=-1), end=tt.Time.from_now(weeks=1),
                                              limit=limit)['trades']
    assert len(response) == limit

