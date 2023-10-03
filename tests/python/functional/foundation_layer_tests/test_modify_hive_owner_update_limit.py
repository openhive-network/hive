import pytest

import test_tools as tt
from hive_local_tools.constants import ALTERNATE_CHAIN_JSON_FILENAME
from hive_local_tools.functional.python import change_hive_owner_update_limit


@pytest.mark.testnet
@pytest.mark.parametrize("limit", [6, 15, 30, 45, 90, 180, 360, 720])
def test_modify_hive_owner_update_limit(limit):
    change_hive_owner_update_limit(seconds_limit=limit)

    node = tt.InitNode()
    node.run(
        arguments=["--alternate-chain-spec", str(tt.context.get_current_directory() / ALTERNATE_CHAIN_JSON_FILENAME)]
    )

    limit_in_microseconds = node.api.database.get_config()["HIVE_OWNER_UPDATE_LIMIT"]
    assert limit_in_microseconds / 1_000_000 == limit, "The `HIVE_OWNER_UPDATE_LIMIT` was not updated correctly."


@pytest.mark.testnet
@pytest.mark.parametrize(
    "limit",
    [
        5,  # the limit cannot be less than default (6 second),
        7,  # the limit must be divisible by 3,
    ],
)
def test_invalid_hive_owner_update_limit_modification(limit):
    change_hive_owner_update_limit(seconds_limit=limit)

    node = tt.InitNode()
    with pytest.raises(TimeoutError):
        node.run(
            arguments=[
                "--alternate-chain-spec",
                str(tt.context.get_current_directory() / ALTERNATE_CHAIN_JSON_FILENAME),
            ]
        )
