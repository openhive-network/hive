from pathlib import Path
from typing import Tuple

from pytest import fixture

import test_tools as tt


@fixture(scope="package")
def block_log_helper() -> Tuple[tt.BlockLog, int]:
    BLOCK_COUNT = 30

    tt.logger.info(f"preparing block log with {BLOCK_COUNT} blocks")
    node = tt.InitNode()
    node.run(wait_for_live=True)

    node.wait_for_block_with_number(BLOCK_COUNT)
    block_log_length = node.get_last_block_number()

    node.close()
    tt.logger.info(f"prepared block log with {block_log_length} blocks")
    yield node.block_log, block_log_length


@fixture(scope="package")
def block_log(block_log_helper) -> Path:
    return block_log_helper[0].path


@fixture(scope="package")
def block_log_length(block_log_helper) -> int:
    return block_log_helper[1]


@fixture
def node_with_20k_proposal_votes():
    block_log_directory = Path(__file__).parent / "block_log"
    block_log_path = block_log_directory / "block_log"
    timestamp_path = block_log_directory / "timestamp"

    with open(timestamp_path, encoding="utf-8") as file:
        timestamp = tt.Time.parse(file.read())

    timestamp -= tt.Time.seconds(5)

    init_node = tt.InitNode()

    init_node.run(
        time_offset=tt.Time.serialize(timestamp, format_=tt.Time.TIME_OFFSET_FORMAT),
        replay_from=block_log_path,
    )

    return init_node
