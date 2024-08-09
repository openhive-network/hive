from __future__ import annotations

from pathlib import Path

import pytest

import test_tools as tt
from hive_local_tools.constants import UNIVERSAL_BLOCK_LOGS_PATH
from python.functional.witness_tests.block_size_set_as_witness_set_properties_tests.block_log.generate_block_log import (
    WITNESSES,
)


@pytest.fixture()
def replay_node() -> tt.InitNode:
    node = tt.InitNode()
    node.config.witness.extend(WITNESSES)
    node.run(
        time_control=tt.StartTimeControl(start_time="head_block_time", speed_up_rate=5),
        replay_from=Path(__file__).parent.joinpath("block_log/block_log"),
        alternate_chain_specs=tt.AlternateChainSpecs.parse_file(
            Path(__file__).parent.joinpath("block_log/alternate-chain-spec.json")
        ),
    )

    assert node.api.database.get_dynamic_global_properties().maximum_block_size == 131072

    return node


@pytest.fixture()
def alternate_chain_spec(block_log):
    return tt.AlternateChainSpecs.parse_file(
        UNIVERSAL_BLOCK_LOGS_PATH / "block_log_single_sign" / tt.AlternateChainSpecs.FILENAME
    )


@pytest.fixture()
def block_log():
    return tt.BlockLog(path=UNIVERSAL_BLOCK_LOGS_PATH / "block_log_single_sign" / "block_log")
