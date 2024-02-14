from __future__ import annotations

from pathlib import Path

import pytest

import test_tools as tt


@pytest.fixture()
def block_log_helper() -> tuple[tt.BlockLog, int]:
    block_count = 30

    tt.logger.info(f"preparing block log with {block_count} blocks")
    node = tt.InitNode()
    node.run(wait_for_live=True)

    node.wait_for_block_with_number(block_count)
    block_log_length = node.get_last_block_number()

    node.close()
    tt.logger.info(f"prepared block log with {block_log_length} blocks")
    return node.block_log, block_log_length


@pytest.fixture()
def block_log(block_log_helper) -> Path:
    return block_log_helper[0].path


@pytest.fixture()
def block_log_length(block_log_helper) -> int:
    return block_log_helper[1]


@pytest.fixture(scope="session")
def block_log_with_artifacts() -> tt.BlockLog:
    """Artifacts must be generated before running tests in parallel to avoid race conditions."""
    block_log_directory = Path(__file__).parent.joinpath("block_log")
    block_log = tt.BlockLog(block_log_directory / "block_log")
    block_log.generate_artifacts()
    return block_log


@pytest.fixture()
def node_with_20k_proposal_votes(block_log_with_artifacts: tt.BlockLog) -> tt.InitNode:
    timestamp = block_log_with_artifacts.get_head_block_time() - tt.Time.seconds(5)

    init_node = tt.InitNode()
    init_node.config.plugin.append("condenser_api")
    init_node.config.plugin.append("market_history_api")

    init_node.run(
        time_offset=tt.Time.serialize(timestamp, format_=tt.TimeFormats.TIME_OFFSET_FORMAT),
        replay_from=block_log_with_artifacts,
    )

    return init_node
