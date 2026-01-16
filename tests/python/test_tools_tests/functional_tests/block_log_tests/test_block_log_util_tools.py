from __future__ import annotations

import tempfile

import pytest
import test_tools as tt
from test_tools.__private.exceptions import BlockLogUtilError


@pytest.fixture
def node() -> tt.InitNode:
    node = tt.InitNode()
    node.run(time_control=tt.SpeedUpRateTimeControl(speed_up_rate=10))
    node.wait_for_block_with_number(30)
    node.restart(time_control=tt.StartTimeControl(start_time="head_block_time"))
    return node


def test_get_head_block_number(node: tt.InitNode) -> None:
    block_log = node.block_log

    head_block_number_from_node = node.get_last_block_number()
    node.close()
    assert (
        block_log.get_head_block_number() == head_block_number_from_node
    ), "Head_block_number from block_log is other than head_block_number from node"


def test_get_block(node: tt.InitNode) -> None:
    block_log = node.block_log

    block_from_node = node.api.block.get_block(block_num=10)
    node.close()

    block_from_block_log = block_log.get_block(block_number=10)
    assert (
        block_from_node.ensure.block.previous == block_from_block_log.previous
    ), "Get_block from block_log getting other block than get_block from node"


def test_get_head_block_time(node: tt.InitNode) -> None:
    block_log = node.block_log

    head_block_time_from_node = node.get_head_block_time()
    node.close()

    head_block_time_from_block_log = block_log.get_head_block_time()
    head_block_time_from_block_log_serialized = block_log.get_head_block_time(
        serialize=True, serialize_format=tt.TimeFormats.FAKETIME_FORMAT
    )

    assert (
        head_block_time_from_node == head_block_time_from_block_log
    ), "Head_block_time from block_log is other than head_block_time from node"

    assert (
        tt.Time.serialize(time=head_block_time_from_node, format_=tt.TimeFormats.FAKETIME_FORMAT)
        == head_block_time_from_block_log_serialized
    ), "Serialized head_block_time from block_log is other than serialized head_block_time from node"


def __generate_artifacts_helper(block_log: tt.BlockLog, kind: str) -> None:
    for file in block_log.artifact_files:
        file.unlink()

    assert not block_log.artifact_files
    block_log.generate_artifacts()
    assert block_log.artifact_files, f"{kind} block log artifacts were not generated"


def test_generate_artifacts(block_log_empty_30_mono: tt.BlockLog, block_log_empty_430_split: tt.BlockLog) -> None:
    __generate_artifacts_helper(block_log_empty_430_split, "Split")
    __generate_artifacts_helper(block_log_empty_30_mono, "Monolithic")


def __exception_handling_helper(block_log: tt.BlockLog, kind: str) -> None:
    with pytest.raises(BlockLogUtilError) as error:
        block_log.get_block("incorrect_block_num")  # type: ignore

    assert "Block incorrect_block_num not found or response malformed:" in str(
        error.value
    ), f"Incorrect error message from {kind} block log"


def test_exception_handling(block_log_empty_30_mono: tt.BlockLog, block_log_empty_430_split: tt.BlockLog) -> None:
    __exception_handling_helper(block_log_empty_430_split, "split")
    __exception_handling_helper(block_log_empty_30_mono, "monolithic")


def test_get_block_ids(node: tt.InitNode) -> None:
    block_log = node.block_log

    block_id_from_node = node.api.block.get_block(block_num=10)
    node.close()

    block_id_from_block_log = block_log.get_block_ids(block_number=10)
    assert (
        block_id_from_node.ensure.block.block_id == block_id_from_block_log
    ), "The block_id in node differs from the block_id in block_log."


@pytest.mark.parametrize(
    ("block_log", "truncate_block_number"),
    [("block_log_empty_30_mono", 20), ("block_log_empty_430_split", 420), ("block_log_empty_430_split", 30)],
)
def test_truncate(block_log: str, truncate_block_number: int, request: pytest.FixtureRequest) -> None:
    truncated_block_log = request.getfixturevalue(block_log).truncate(tempfile.gettempdir(), truncate_block_number)
    assert (
        truncated_block_log.get_head_block_number() == truncate_block_number
    ), "Truncated block log head block number does NOT match truncate block number."
