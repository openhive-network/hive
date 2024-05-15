from __future__ import annotations

from typing import TYPE_CHECKING, Any

from requests import post

from hive_local_tools import run_for

if TYPE_CHECKING:
    import test_tools as tt


@run_for("testnet")
def test_uniqueness_of_transaction_ids_generated_by_wallet(node: tt.InitNode | tt.RemoteNode, wallet: tt.Wallet):
    already_generated_transaction_ids = set()

    for name in [f"account-{i:02d}" for i in range(100)]:
        transaction = wallet.api.create_account("initminer", name, "{}", broadcast=False)

        assert transaction["transaction_id"] not in already_generated_transaction_ids
        already_generated_transaction_ids.add(transaction["transaction_id"])


# In this test case it is important that all generated transactions go to single block.
# We are unable to guarantee this due to the race between blocks production and generating transactions.
# If the transactions do not enter a single block, they will be split between two blocks.
# In this case, the test is repeated.
@run_for("testnet")
def test_if_transaction_ids_order_corresponds_to_transactions_order(
    node: tt.InitNode | tt.RemoteNode, wallet: tt.Wallet
):
    names = [f"account-{i:02d}" for i in range(20)]

    # transactions created in this loop are sent to single block (it's ensured by assert below)
    batch_send(node, [wallet.api.create_account("initminer", name, "{}", broadcast=False) for name in names])

    node.wait_number_of_blocks(1)  # waiting for the above transactions to be in the block log
    block = node.api.condenser.get_block(node.get_last_block_number())
    # make sure that all created transactions go into single block
    assert len(block["transactions"]) == len(names)

    for transaction_id, transaction in zip(block["transaction_ids"], block["transactions"]):
        assert transaction_id == transaction["transaction_id"]


def batch_send(node: tt.InitNode | tt.RemoteNode, requests: list[dict[str, Any]]) -> None:
    [
        {"jsonrpc": "2.0", "method": "condenser_api.broadcast_transaction", "id": id_, "params": [request]}
        for id_, request in enumerate(requests)
    ]
    assert post(node.http_endpoint.as_string()).status_code == 200
    # TODO: Update this with batch send
