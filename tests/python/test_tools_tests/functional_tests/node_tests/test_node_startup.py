from __future__ import annotations

from typing import TYPE_CHECKING

import pytest
import test_tools as tt

from wax.helpy import Time

if TYPE_CHECKING:
    from test_tools.__private.type_annotations.any_node import AnyNode


def test_init_node_startup(node: tt.InitNode) -> None:
    node.wait_for_block_with_number(1)


def test_startup_timeout() -> None:
    node = tt.WitnessNode(witnesses=[])
    with pytest.raises(TimeoutError):
        node.run(timeout=2)


def test_loading_own_snapshot(node: tt.InitNode) -> None:
    make_transaction_for_test(node)
    generate_blocks(node, 100)  # To make sure, that block with test operation will be stored in block log
    snapshot = node.dump_snapshot(close=True)

    # Node is not waiting for live here, because of blocks generation behavior.
    # After N blocks generation, node will stop blocks production for N * 3s.
    node.run(
        time_control=tt.StartTimeControl(start_time="head_block_time"), load_snapshot_from=snapshot, wait_for_live=False
    )
    assert_that_transaction_for_test_has_effect(node)


def test_loading_snapshot_from_other_node(node: tt.InitNode) -> None:
    make_transaction_for_test(node)
    generate_blocks(node, 22)  # To make sure, that block with test operation will be stored in block log
    snapshot = node.dump_snapshot()

    loading_node = tt.InitNode()
    loading_node.run(load_snapshot_from=snapshot, wait_for_live=False)
    assert_that_transaction_for_test_has_effect(loading_node)


def test_loading_snapshot_from_custom_path(node: tt.InitNode) -> None:
    make_transaction_for_test(node)
    generate_blocks(node, 22)  # To make sure, that block with test operation will be stored in block log
    snapshot = node.dump_snapshot()

    loading_node = tt.InitNode()
    loading_node.run(load_snapshot_from=snapshot.get_path(), wait_for_live=False)
    assert_that_transaction_for_test_has_effect(loading_node)


def test_replay_from_other_node_block_log(node: tt.InitNode) -> None:
    make_transaction_for_test(node)
    generate_blocks(node, 100)  # To make sure, that block with test operation will be stored in block log
    node.close()

    replaying_node = tt.ApiNode()
    replaying_node.run(
        time_control=tt.StartTimeControl(start_time="head_block_time"), replay_from=node.block_log, wait_for_live=False
    )
    assert_that_transaction_for_test_has_effect(replaying_node)


def test_replay_until_specified_block(node: tt.InitNode) -> None:
    expected_number_of_blocks = 50

    generate_blocks(node, 100)
    node.close()

    replaying_node = tt.ApiNode()
    replaying_node.run(
        time_control=tt.StartTimeControl(start_time="head_block_time"),
        replay_from=node.block_log,
        stop_at_block=50,
        wait_for_live=False,
    )
    assert replaying_node.get_last_block_number() == expected_number_of_blocks


def test_replay_from_external_block_log(node: tt.InitNode) -> None:
    expected_number_of_blocks = 50

    generate_blocks(node, 100)
    node.close()

    # Rename block log, to check if block logs with changed names are also handled.
    external_block_log = node.block_log.copy_to(tt.context.get_current_directory() / "external_block_log")

    replaying_node = tt.ApiNode()
    replaying_node.run(
        time_control=tt.StartTimeControl(start_time="head_block_time"),
        replay_from=external_block_log.path,
        stop_at_block=50,
        wait_for_live=False,
    )
    assert replaying_node.get_last_block_number() == expected_number_of_blocks


def test_exit_before_synchronization() -> None:
    init_node = tt.InitNode()
    init_node.run(exit_before_synchronization=True)
    assert not init_node.is_running()


def test_restart(node: tt.InitNode) -> None:
    node.restart()


def test_startup_with_modified_time() -> None:
    requested_start_time = Time.parse("2020-01-01T00:00:00")

    init_node = tt.InitNode()
    init_node.run(time_control=tt.StartTimeControl(start_time=requested_start_time))

    node_time = Time.parse(init_node.api.database.get_dynamic_global_properties()["time"])

    # Some time may pass between node start and getting time, so below comparison is made with a few block tolerance.
    assert Time.are_close(requested_start_time, node_time, absolute_tolerance=Time.seconds(15))


def make_transaction_for_test(node: AnyNode) -> None:
    wallet = tt.Wallet(attach_to=node)
    wallet.api.create_account("initminer", "alice", "{}")


def assert_that_transaction_for_test_has_effect(node: AnyNode) -> None:
    response = node.api.database.find_accounts(accounts=["alice"], delayed_votes_active=False)
    assert response.accounts[0].name == "alice"


def generate_blocks(node: tt.InitNode, number_of_blocks: int) -> None:
    tt.logger.info(f"Generation of {number_of_blocks} blocks started...")
    node.api.debug_node.debug_generate_blocks(
        debug_key=tt.Account("initminer").private_key,
        count=number_of_blocks,
        skip=0,
        miss_blocks=0,
    )
    tt.logger.info("Blocks generation finished.")
