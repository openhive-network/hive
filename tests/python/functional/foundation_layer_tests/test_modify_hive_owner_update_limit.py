from __future__ import annotations

import pytest

import test_tools as tt
from beekeepy.exceptions import FailedToStartExecutableError


@pytest.mark.testnet()
@pytest.mark.parametrize("limit", [6, 15, 30, 45, 90, 180, 360, 720])
def test_modify_hive_owner_update_limit(limit: int) -> None:
    node = tt.InitNode()
    current_hardfork_number = int(node.get_version()["version"]["blockchain_version"].split(".")[1])

    node = tt.InitNode()
    node.run(
        alternate_chain_specs=tt.AlternateChainSpecs(
            genesis_time=int(tt.Time.now(serialize=False).timestamp()),
            hardfork_schedule=[tt.HardforkSchedule(hardfork=current_hardfork_number, block_num=1)],
            hive_owner_update_limit=limit,
        )
    )

    limit_in_microseconds = node.api.database.get_config().HIVE_OWNER_UPDATE_LIMIT
    assert limit_in_microseconds / 1_000_000 == limit, "The `HIVE_OWNER_UPDATE_LIMIT` was not updated correctly."


@pytest.mark.testnet()
@pytest.mark.parametrize(
    "limit",
    [
        5,  # the limit cannot be less than default (6 second),
        7,  # the limit must be divisible by 3,
    ],
)
def test_invalid_hive_owner_update_limit_modification(limit: int) -> None:
    node = tt.InitNode()
    current_hardfork_number = int(node.get_version()["version"]["blockchain_version"].split(".")[1])

    with pytest.raises(FailedToStartExecutableError):
        node.run(
            alternate_chain_specs=tt.AlternateChainSpecs(
                genesis_time=int(tt.Time.now(serialize=False).timestamp()),
                hardfork_schedule=[tt.HardforkSchedule(hardfork=current_hardfork_number, block_num=1)],
                hive_owner_update_limit=limit,
            )
        )
