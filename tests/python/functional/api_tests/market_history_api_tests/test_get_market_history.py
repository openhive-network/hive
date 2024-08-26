from __future__ import annotations

import datetime

import pytest

import test_tools as tt
from hive_local_tools import run_for


@pytest.mark.parametrize("execution_number", range(1000))
@pytest.mark.parametrize(
    ("tbds", "hive_high", "non_hive_high", "hive_low", "non_hive_low", "non_hive_volume"),
    [
        (25, 300_000, 40_000, 100_000, 10_000, 75_000),
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
    execution_number,
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
