from __future__ import annotations

import datetime

import pytest

import test_tools as tt
from hive_local_tools import run_for


@pytest.mark.skip(reason="https://gitlab.syncad.com/hive/hive/-/issues/449")
@run_for("testnet")
def test_get_market_history_with_start_date_after_end(node: tt.InitNode) -> None:
    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.market_history.get_market_history(
            bucket_seconds=60, start=tt.Time.from_now(weeks=10), end=tt.Time.from_now(weeks=-1)
        )


@run_for("testnet", enable_plugins=["market_history_api"])
def test_exceed_time_range(node: tt.InitNode) -> None:
    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.market_history.get_market_history(
            bucket_seconds=60, start=tt.Time.from_now(years=-100), end=tt.Time.from_now(years=100)
        )


@pytest.mark.parametrize("bucket_seconds", [1, 14, 32, 59, 61, 299, 301, 3599, 86405])
@pytest.mark.skip(reason="https://gitlab.syncad.com/hive/hive/-/issues/450")
@run_for("testnet")
def test_get_market_history_with_wrong_bucket_seconds_value(node: tt.InitNode | tt.RemoteNode, bucket_seconds: int):
    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.market_history.get_market_history(
            bucket_seconds=bucket_seconds, start=tt.Time.from_now(weeks=-1), end=tt.Time.from_now(weeks=1)
        )


@run_for("testnet", enable_plugins=["market_history_api"])
def test_create_better_offer_and_cancel_it(node: tt.InitNode) -> None:
    wallet = tt.Wallet(attach_to=node)
    wallet.create_account("alice", hives=tt.Asset.Test(1000), vests=tt.Asset.Test(100))
    wallet.create_account("bob", hives=tt.Asset.Test(1000), vests=tt.Asset.Test(100))

    # initial order - hives to hbd
    wallet.api.create_order("alice", 0, tt.Asset.Test(300), tt.Asset.Tbd(30), False, 3600)
    # create order with lower price
    wallet.api.create_order("bob", 1, tt.Asset.Test(301), tt.Asset.Tbd(20), False, 3600)
    wallet.api.cancel_order("bob", 1)
    # opposite order - sell hbd for hives
    wallet.api.create_order("initminer", 2, tt.Asset.Tbd(100), tt.Asset.Test(1000), False, 3600)
    buckets = node.api.market_history.get_market_history(
        bucket_seconds=15, start=tt.Time.from_now(weeks=-1), end=tt.Time.from_now(weeks=1)
    )["buckets"]
    assert buckets[0]["hive"]["low"] != 301000
    assert buckets[0]["non_hive"]["low"] != 20000


@run_for("testnet", enable_plugins=["market_history_api"])
def test_get_empty_market_history(node: tt.InitNode) -> None:
    wallet = tt.Wallet(attach_to=node)
    wallet.create_account("alice", hives=tt.Asset.Test(500), vests=tt.Asset.Test(100))

    wallet.api.create_order("alice", 0, tt.Asset.Test(300), tt.Asset.Tbd(30), False, 3600)
    wallet.api.create_order("initminer", 2, tt.Asset.Tbd(100), tt.Asset.Test(1000), False, 3600)

    # wait 6 blocks - more than 15 seconds
    node.wait_number_of_blocks(6)
    # ask for history from last 15 seconds
    buckets = node.api.market_history.get_market_history(
        bucket_seconds=15, start=tt.Time.from_now(seconds=-15), end=tt.Time.from_now(weeks=1)
    )["buckets"]
    assert len(buckets) == 0


@pytest.mark.parametrize(
    # 6, 22 blocks - more than 15 and 60 seconds - to exceed bucket time.
    ("bucket_seconds", "blocks_to_wait"),
    [
        (15, 6),
        (60, 22),
    ],
)
@run_for("testnet", enable_plugins=["market_history_api"])
def test_get_two_buckets(node: tt.InitNode, bucket_seconds: int, blocks_to_wait: int) -> None:
    wallet = tt.Wallet(attach_to=node)
    wallet.create_account("alice", hives=tt.Asset.Test(500), vests=tt.Asset.Test(100))

    wallet.api.create_order("alice", 0, tt.Asset.Test(300), tt.Asset.Tbd(30), False, 3600)
    wallet.api.create_order("initminer", 1, tt.Asset.Tbd(100), tt.Asset.Test(1000), False, 3600)

    node.wait_number_of_blocks(blocks_to_wait)

    wallet.api.create_order("alice", 2, tt.Asset.Test(100), tt.Asset.Tbd(10), False, 3600)
    buckets = node.api.market_history.get_market_history(
        bucket_seconds=bucket_seconds, start=tt.Time.from_now(weeks=-1), end=tt.Time.from_now(weeks=1)
    )["buckets"]
    assert len(buckets) == 2


@pytest.mark.parametrize(
    ("tbds", "hive_high", "non_hive_high", "hive_low", "non_hive_low", "non_hive_volume"),
    [
        (40, 200_000, 40_000, 100_000, 10_000, 90_000),
        (25, 300_000, 40_000, 100_000, 10_000, 75_000),
        (8, 300_000, 40_000, 200_000, 8_000, 58_000),
    ],
)
@run_for("testnet", enable_plugins=["market_history_api"])
def test_bucket_output_parameters(
    node: tt.InitNode,
    tbds: int,
    hive_high: int,
    hive_low: int,
    non_hive_high: int,
    non_hive_low: int,
    non_hive_volume: int,
) -> None:
    wallet = tt.Wallet(attach_to=node)
    wallet.create_account("alice", hives=tt.Asset.Test(1000), vests=tt.Asset.Test(100))
    epoch_time_zero = tt.Time.parse("1970-01-01T00:00:00")
    time_needed_for_transactions = 15  # in seconds
    transactions_done = False

    while not transactions_done:
        hbt = node.get_head_block_time()
        open_in_seconds_from_epoch_time_zero = (
            (hbt - epoch_time_zero).total_seconds() // 300
        ) * 300  # time when current bucket opens at
        bucket_opens = epoch_time_zero + datetime.timedelta(seconds=open_in_seconds_from_epoch_time_zero)
        bucket_closes = bucket_opens + datetime.timedelta(seconds=300)
        # if difference between head block time and when current bucket closes is enough, broadcast transactions
        if (bucket_closes - node.get_head_block_time()).seconds >= time_needed_for_transactions + 10:
            start_block = node.get_last_block_number()
            tt.logger.info(f"Starting order creation at block: {start_block}")  # Usually starts at block #3
            wallet.api.create_order("alice", 0, tt.Asset.Test(100), tt.Asset.Tbd(10), False, 3600)
            wallet.api.create_order("initminer", 1, tt.Asset.Tbd(10), tt.Asset.Test(100), False, 3600)

            wallet.api.create_order("alice", 2, tt.Asset.Test(200), tt.Asset.Tbd(tbds), False, 3600)
            wallet.api.create_order("initminer", 3, tt.Asset.Tbd(tbds), tt.Asset.Test(200), False, 3600)

            wallet.api.create_order("alice", 4, tt.Asset.Test(300), tt.Asset.Tbd(40), False, 3600)
            wallet.api.create_order("initminer", 5, tt.Asset.Tbd(40), tt.Asset.Test(300), False, 3600)

            end_block = node.get_last_block_number()  # Usually ends at block #9
            tt.logger.info(f"Order creation finished at block: {end_block}")
            transactions_done = True
        else:
            tt.logger.info(
                "There is no enough time left in current bucket to create transactions. Waiting for new "
                "bucket to open..."
            )
            while node.get_head_block_time() < bucket_closes:
                node.wait_number_of_blocks(1)
    buckets = node.api.market_history.get_market_history(
        bucket_seconds=300,
        start=tt.Time.from_now(weeks=-1),
        end=tt.Time.from_now(weeks=1),
    ).buckets

    assert len(buckets) == 1
    assert buckets[0]["hive"]["open"] == 100_000
    assert buckets[0]["non_hive"]["open"] == 10_000

    assert buckets[0]["hive"]["close"] == 300_000
    assert buckets[0]["non_hive"]["close"] == 40_000

    assert buckets[0]["hive"]["volume"] == 600_000
    assert buckets[0]["non_hive"]["volume"] == non_hive_volume

    assert buckets[0]["hive"]["high"] == hive_high
    assert buckets[0]["non_hive"]["high"] == non_hive_high

    assert buckets[0]["hive"]["low"] == hive_low
    assert buckets[0]["non_hive"]["low"] == non_hive_low
