import pytest

import test_tools as tt
from hive_local_tools import run_for


@pytest.mark.skip(reason='https://gitlab.syncad.com/hive/hive/-/issues/449')
@run_for('testnet')
def test_get_market_history_with_start_date_after_end(node):
    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.market_history.get_market_history(bucket_seconds=60, start=tt.Time.from_now(weeks=10),
                                                   end=tt.Time.from_now(weeks=-1))


@run_for('testnet')
def test_exceed_time_range(node):
    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.market_history.get_market_history(bucket_seconds=60, start=tt.Time.from_now(years=-100),
                                                   end=tt.Time.from_now(years=100))


@pytest.mark.parametrize(
    'bucket_seconds', (
        1, 14, 32, 59, 61, 299, 301, 3599, 86405
    )
)
@pytest.mark.skip(reason='https://gitlab.syncad.com/hive/hive/-/issues/450')
@run_for('testnet')
def test_get_market_history_with_wrong_bucket_seconds_value(node, bucket_seconds):
    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.market_history.get_market_history(bucket_seconds=bucket_seconds, start=tt.Time.from_now(weeks=-1),
                                                   end=tt.Time.from_now(weeks=1))


@run_for('testnet')
def test_create_better_offer_and_cancel_it(node):
    wallet = tt.Wallet(attach_to=node)
    wallet.create_account('alice', hives=tt.Asset.Test(1000), vests=tt.Asset.Test(100))
    wallet.create_account('bob', hives=tt.Asset.Test(1000), vests=tt.Asset.Test(100))

    # initial order - hives to hbd
    wallet.api.create_order('alice', 0, tt.Asset.Test(300), tt.Asset.Tbd(30), False, 3600)
    # create order with lower price
    wallet.api.create_order('bob', 1, tt.Asset.Test(301), tt.Asset.Tbd(20), False, 3600)
    wallet.api.cancel_order('bob', 1)
    # opposite order - sell hbd for hives
    wallet.api.create_order('initminer', 2, tt.Asset.Tbd(100), tt.Asset.Test(1000), False, 3600)
    buckets = node.api.market_history.get_market_history(bucket_seconds=15, start=tt.Time.from_now(weeks=-1),
                                                         end=tt.Time.from_now(weeks=1))['buckets']
    assert buckets[0]['hive']['low'] != 301000
    assert buckets[0]['non_hive']['low'] != 20000


@run_for('testnet')
def test_get_empty_market_history(node):
    wallet = tt.Wallet(attach_to=node)
    wallet.create_account('alice', hives=tt.Asset.Test(500), vests=tt.Asset.Test(100))

    wallet.api.create_order('alice', 0, tt.Asset.Test(300), tt.Asset.Tbd(30), False, 3600)
    wallet.api.create_order('initminer', 2, tt.Asset.Tbd(100), tt.Asset.Test(1000), False, 3600)

    # wait 6 blocks - more than 15 seconds
    node.wait_number_of_blocks(6)
    # ask for history from last 15 seconds
    buckets = node.api.market_history.get_market_history(bucket_seconds=15, start=tt.Time.from_now(seconds=-15),
                                                         end=tt.Time.from_now(weeks=1))['buckets']
    assert len(buckets) == 0


@pytest.mark.parametrize(
    # 6, 22 blocks - more than 15 and 60 seconds - to exceed bucket time.
    'bucket_seconds, blocks_to_wait', (
            (15, 6),
            (60, 22),
    )
)
@run_for('testnet')
def test_get_two_buckets(node, bucket_seconds, blocks_to_wait):
    wallet = tt.Wallet(attach_to=node)
    wallet.create_account('alice', hives=tt.Asset.Test(500), vests=tt.Asset.Test(100))

    wallet.api.create_order('alice', 0, tt.Asset.Test(300), tt.Asset.Tbd(30), False, 3600)
    wallet.api.create_order('initminer', 1, tt.Asset.Tbd(100), tt.Asset.Test(1000), False, 3600)

    node.wait_number_of_blocks(blocks_to_wait)

    wallet.api.create_order('alice', 2, tt.Asset.Test(100), tt.Asset.Tbd(10), False, 3600)
    buckets = node.api.market_history.get_market_history(bucket_seconds=bucket_seconds, start=tt.Time.from_now(weeks=-1),
                                                         end=tt.Time.from_now(weeks=1))['buckets']
    assert len(buckets) == 2



@pytest.mark.parametrize(
    'tbds, hive_high, non_hive_high, hive_low, non_hive_low, non_hive_volume',
    (
        (40, 200_000, 40_000, 100_000, 10_000, 90_000),
        (25, 300_000, 40_000, 100_000, 10_000, 75_000),
        (8, 300_000, 40_000, 200_000, 8_000, 58_000),
    )
)
@run_for('testnet')
def test_bucket_output_parameters(node, tbds, hive_high, hive_low, non_hive_high, non_hive_low, non_hive_volume):
    wallet = tt.Wallet(attach_to=node)
    wallet.create_account('alice', hives=tt.Asset.Test(1000), vests=tt.Asset.Test(100))

    wallet.api.create_order('alice', 0, tt.Asset.Test(100), tt.Asset.Tbd(10), False, 3600)
    wallet.api.create_order('initminer', 1, tt.Asset.Tbd(10), tt.Asset.Test(100), False, 3600)

    wallet.api.create_order('alice', 2, tt.Asset.Test(200), tt.Asset.Tbd(tbds), False, 3600)
    wallet.api.create_order('initminer', 3, tt.Asset.Tbd(tbds), tt.Asset.Test(200), False, 3600)

    wallet.api.create_order('alice', 4, tt.Asset.Test(300), tt.Asset.Tbd(40), False, 3600)
    wallet.api.create_order('initminer', 5, tt.Asset.Tbd(40), tt.Asset.Test(300), False, 3600)

    buckets = node.api.market_history.get_market_history(bucket_seconds=300, start=tt.Time.from_now(weeks=-1),
                                                         end=tt.Time.from_now(weeks=1))['buckets']

    assert len(buckets) == 1
    assert buckets[0]['hive']['open'] == 100_000
    assert buckets[0]['non_hive']['open'] == 10_000

    assert buckets[0]['hive']['close'] == 300_000
    assert buckets[0]['non_hive']['close'] == 40_000

    assert buckets[0]['hive']['volume'] == 600_000
    assert buckets[0]['non_hive']['volume'] == non_hive_volume

    assert buckets[0]['hive']['high'] == hive_high
    assert buckets[0]['non_hive']['high'] == non_hive_high

    assert buckets[0]['hive']['low'] == hive_low
    assert buckets[0]['non_hive']['low'] == non_hive_low
