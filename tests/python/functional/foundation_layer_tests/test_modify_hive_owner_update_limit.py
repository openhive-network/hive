from __future__ import annotations

import pytest

import test_tools as tt
from hive_local_tools import create_alternate_chain_spec_file
from hive_local_tools.constants import ALTERNATE_CHAIN_JSON_FILENAME


@pytest.mark.testnet()
@pytest.mark.parametrize("limit", [6, 15, 30, 45, 90, 180, 360, 720])
def test_modify_hive_owner_update_limit(limit):
    node = tt.InitNode()
    current_hardfork_number = int(node.get_version()["version"]["blockchain_version"].split(".")[1])

    create_alternate_chain_spec_file(
        genesis_time=int(tt.Time.now(serialize=False).timestamp()),
        hardfork_schedule=[{"hardfork": current_hardfork_number, "block_num": 1}],
        hive_owner_update_limit=limit,
    )

    node = tt.InitNode()
    node.run(
        arguments=["--alternate-chain-spec", str(tt.context.get_current_directory() / ALTERNATE_CHAIN_JSON_FILENAME)]
    )

    limit_in_microseconds = node.api.database.get_config()["HIVE_OWNER_UPDATE_LIMIT"]
    assert limit_in_microseconds / 1_000_000 == limit, "The `HIVE_OWNER_UPDATE_LIMIT` was not updated correctly."


@pytest.mark.testnet()
@pytest.mark.parametrize(
    "limit",
    [
        5,  # the limit cannot be less than default (6 second),
        7,  # the limit must be divisible by 3,
    ],
)
def test_invalid_hive_owner_update_limit_modification(limit):
    node = tt.InitNode()
    current_hardfork_number = int(node.get_version()["version"]["blockchain_version"].split(".")[1])

    create_alternate_chain_spec_file(
        genesis_time=int(tt.Time.now(serialize=False).timestamp()),
        hardfork_schedule=[{"hardfork": current_hardfork_number, "block_num": 1}],
        hive_owner_update_limit=limit,
    )

    with pytest.raises(TimeoutError):
        node.run(
            arguments=[
                "--alternate-chain-spec",
                str(tt.context.get_current_directory() / ALTERNATE_CHAIN_JSON_FILENAME),
            ]
        )
