import pytest

from hive_local_tools import run_for


@run_for("testnet")
def test_uniqueness_of_transaction_ids_generated_by_wallet(node, wallet):
    already_generated_transaction_ids = set()

    for name in [f"account-{i:02d}" for i in range(100)]:
        transaction = wallet.api.create_account("initminer", name, "{}", broadcast=False)

        assert transaction["transaction_id"] not in already_generated_transaction_ids
        already_generated_transaction_ids.add(transaction["transaction_id"])


# In this test case it is important that all generated transactions go to single block.
# We are unable to guarantee this due to the race between blocks production and generating transactions.
# If the transactions do not enter a single block, they will be split between two blocks.
# In this case, the test is repeated.
@pytest.mark.flaky(reruns=5, reruns_delay=30)
@run_for("testnet")
def test_if_transaction_ids_order_corresponds_to_transactions_order(node, wallet):
    names = [f"account-{i:02d}" for i in range(20)]

    # transactions created in this loop are sent to single block (it's ensured by assert below)
    for name in names:
        transaction = wallet.api.create_account("initminer", name, "{}", broadcast=False)
        node.api.condenser.broadcast_transaction(transaction)

    node.wait_number_of_blocks(1)  # waiting for the above transactions to be in the block log
    block = node.api.condenser.get_block(node.get_last_block_number())
    # make sure that all created transactions go into single block
    assert len(block["transactions"]) == len(names)

    for transaction_id, transaction in zip(block["transaction_ids"], block["transactions"]):
        assert transaction_id == transaction["transaction_id"]
