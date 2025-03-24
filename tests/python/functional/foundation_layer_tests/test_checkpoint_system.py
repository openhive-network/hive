from __future__ import annotations

import random
from subprocess import CalledProcessError
from typing import Final

import pytest

import test_tools as tt
from hive_local_tools.functional import simultaneous_node_startup
from shared_tools.complex_networks import generate_free_addresses


def set_checkpoint(node: tt.InitNode | tt.ApiNode, block_log: tt.BlockLog, block_num: int) -> None:
    node.config.checkpoint.append((block_num, block_log.get_block_ids(block_num)))


def verify_error_in_stderr(node: tt.InitNode | tt.ApiNode, error_message: str) -> AssertionError | None:
    with open(node.directory / "stderr.txt") as file:
        stderr = file.read()
        assert error_message in stderr


def generate_random_hex_string(n: int) -> str:
    return "".join(random.choices("0123456789abcdef", k=n))


@pytest.mark.parametrize("checkpoint_num", [0, 1, 10, 100, 429])
def test_set_random_number_of_checkpoints_from_replay(
    block_log_empty_430_split: tt.BlockLog, checkpoint_num: int
) -> None:
    node = tt.InitNode()

    for num in random.sample(range(1, block_log_empty_430_split.get_head_block_number() + 1), checkpoint_num):
        set_checkpoint(node, block_log_empty_430_split, num)

    node.run(replay_from=block_log_empty_430_split, stop_at_block=block_log_empty_430_split.get_head_block_number())

    assert node.is_running()


@pytest.mark.parametrize("checkpoint_num", [0, 1, 10, 100, 429])
def test_set_random_number_of_checkpoints_from_sync(
    block_log_empty_430_split: tt.BlockLog, checkpoint_num: int
) -> None:
    node = tt.InitNode()
    node.run(replay_from=block_log_empty_430_split, exit_before_synchronization=True)

    api_node = tt.ApiNode()

    node.config.p2p_endpoint = generate_free_addresses(1)[0]
    api_node.config.p2p_seed_node.append(node.config.p2p_endpoint)

    for num in random.sample(range(1, block_log_empty_430_split.get_head_block_number() + 1), checkpoint_num):
        set_checkpoint(api_node, block_log_empty_430_split, num)

    simultaneous_node_startup([node, api_node], timeout=120, wait_for_live=True)

    assert node.is_running()
    assert api_node.is_running()


def test_start_replay_from_checkpoint(block_log_empty_430_split: tt.BlockLog) -> None:
    node = tt.InitNode()
    node.run(replay_from=block_log_empty_430_split, exit_at_block=100)
    assert not node.is_running()

    checkpoint_block: Final[int] = 100
    checkpoint_id = block_log_empty_430_split.get_block_ids(checkpoint_block)

    node.config.checkpoint.append((checkpoint_block, checkpoint_id))

    node.run(replay_from=block_log_empty_430_split)

    assert node.is_running()


def test_stop_replay_at_checkpoint(block_log_empty_430_split: tt.BlockLog) -> None:
    node = tt.InitNode()

    set_checkpoint(node, block_log_empty_430_split, block_log_empty_430_split.get_head_block_number())

    node.run(replay_from=block_log_empty_430_split, stop_at_block=block_log_empty_430_split.get_head_block_number())

    assert (
        node.get_last_block_number() == block_log_empty_430_split.get_head_block_number()
    ), "Node did not stop at a checkpoint."
    assert node.is_running()


def test_checkpoint_id_mismatch_with_replay_block_id(block_log_empty_430_split: tt.BlockLog) -> None:
    node = tt.InitNode()

    checkpoint_block: Final[int] = 10
    correct_id: str = block_log_empty_430_split.get_block_ids(checkpoint_block)
    id_unmatched_to_block: Final[str] = correct_id.replace(correct_id[-3:], "000")

    node.config.checkpoint.append((checkpoint_block, id_unmatched_to_block))

    with pytest.raises(CalledProcessError):
        node.run(replay_from=block_log_empty_430_split, exit_before_synchronization=True)

    assert not node.is_running()

    verify_error_in_stderr(
        node,
        error_message=(
            "Block did not match checkpoint (unformatted args:"
            f' ("checkpoint",[{checkpoint_block},"{id_unmatched_to_block}"])'
        ),
    )


def test_checkpoint_invalid_block_id_mismatch(block_log_empty_430_split: tt.BlockLog) -> None:
    node = tt.InitNode()
    checkpoint_block: Final[int] = 10
    node.config.checkpoint.append((checkpoint_block, block_log_empty_430_split.get_block_ids(checkpoint_block + 1)))

    with pytest.raises(CalledProcessError):
        node.run(replay_from=block_log_empty_430_split, exit_before_synchronization=True)

    assert not node.is_running()

    verify_error_in_stderr(
        node,
        error_message=(
            f"Invalid checkpoint specification, block number ({checkpoint_block}) does not match number "
            f"embedded in block id ({checkpoint_block + 1})"
        ),
    )


@pytest.mark.parametrize("is_valid_checkpoint", [True, False])
def test_checkpoint_past_state_match(block_log_empty_430_split: tt.BlockLog, is_valid_checkpoint: bool) -> None:
    node = tt.InitNode()
    node.run(replay_from=block_log_empty_430_split, exit_at_block=100)
    assert not node.is_running()

    checkpoint_block: Final[int] = 50
    checkpoint_id = block_log_empty_430_split.get_block_ids(checkpoint_block)
    if not is_valid_checkpoint:
        checkpoint_id = checkpoint_id[:-3] + "000"

    node.config.checkpoint.append((checkpoint_block, checkpoint_id))

    node.run(replay_from=block_log_empty_430_split)
    assert node.is_running()


def test_api_node_rejects_mismatched_checkpoint(block_log_empty_430_split: tt.BlockLog) -> None:
    node = tt.InitNode()
    node.run(replay_from=block_log_empty_430_split, exit_before_synchronization=True)

    api_node = tt.ApiNode()

    node.config.p2p_endpoint = generate_free_addresses(1)[0]
    api_node.config.p2p_seed_node.append(node.config.p2p_endpoint)

    checkpoint_block: Final[int] = block_log_empty_430_split.get_head_block_number() + 20
    hex_value = f"{checkpoint_block:08x}"
    id_unmatched_to_block = hex_value + generate_random_hex_string(
        len(block_log_empty_430_split.get_block_ids(1)) - len(hex_value)
    )

    api_node.config.checkpoint.append((checkpoint_block, id_unmatched_to_block))

    simultaneous_node_startup([node, api_node], timeout=120, wait_for_live=True)

    assert node.is_running()
    assert api_node.is_running()

    node.wait_for_block_with_number(checkpoint_block + 5, timeout=120)

    assert api_node.get_last_block_number() == checkpoint_block - 1, "Invalid checkpoint was crossed successfully."

    api_node.close()
    node.close()

    verify_error_in_stderr(
        api_node,
        error_message=(
            "Block did not match checkpoint (unformatted args:"
            f' ("checkpoint",[{checkpoint_block},"{id_unmatched_to_block}"])'
        ),
    )


def test_checkpoint_missmatch_to_block_from_sync(block_log_empty_430_split: tt.BlockLog) -> None:
    node = tt.InitNode()
    node.run(replay_from=block_log_empty_430_split, exit_before_synchronization=True)

    api_node = tt.ApiNode()

    node.config.p2p_endpoint = generate_free_addresses(1)[0]
    api_node.config.p2p_seed_node.append(node.config.p2p_endpoint)

    checkpoint_block: Final[int] = 50
    hex_value = f"{checkpoint_block:08x}"
    id_unmatched_to_block = hex_value + generate_random_hex_string(
        len(block_log_empty_430_split.get_block_ids(1)) - len(hex_value)
    )
    api_node.config.checkpoint.append((checkpoint_block, id_unmatched_to_block))

    simultaneous_node_startup([node, api_node], timeout=120, wait_for_live=False)

    assert node.is_running()
    assert api_node.is_running()

    node.wait_for_block_with_number(block_log_empty_430_split.get_head_block_number() + 10)

    verify_error_in_stderr(
        api_node,
        error_message=(
            "Block did not match checkpoint (unformatted args:"
            f' ("checkpoint",[{checkpoint_block},"{id_unmatched_to_block}"])'
        ),
    )
