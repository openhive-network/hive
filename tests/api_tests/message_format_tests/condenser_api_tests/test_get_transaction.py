import test_tools as tt

from hive_local_tools import run_for


@run_for("testnet")
def test_get_transaction_in_testnet(node, wallet):
    transaction = wallet.api.create_account("initminer", "alice", "{}")

    node.wait_for_irreversible_block()
    node.api.condenser.get_transaction(transaction["transaction_id"])


@run_for("mainnet_5m", "live_mainnet")
def test_get_transaction_in_mainnet(node):
    block = node.api.condenser.get_block(4450001)
    transaction = block["transactions"][0]
    node.api.condenser.get_transaction(transaction["transaction_id"])
