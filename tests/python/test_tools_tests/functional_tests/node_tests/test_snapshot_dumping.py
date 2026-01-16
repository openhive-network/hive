from __future__ import annotations

import test_tools as tt


def test_if_snapshot_is_dumped_and_node_continues_to_run(node: tt.InitNode) -> None:
    node.dump_snapshot()
    assert node.is_running()


def test_if_snapshot_is_dumped_and_node_is_closed(node: tt.InitNode) -> None:
    node.dump_snapshot(close=True)
    assert not node.is_running()


def test_if_operation_could_be_created_right_after_dumping_snapshot(node: tt.InitNode) -> None:
    # ARRANGE
    wallet = tt.Wallet(attach_to=node)

    # ACT
    wallet.create_account("alice")

    # wait for the block with the transaction to become irreversible, and will be saved in block_log
    node.wait_number_of_blocks(21)

    node.dump_snapshot()

    wallet.create_account("bob")

    # ASSERT
    assert node.api.wallet_bridge.list_accounts("", 2) == ["alice", "bob"]
