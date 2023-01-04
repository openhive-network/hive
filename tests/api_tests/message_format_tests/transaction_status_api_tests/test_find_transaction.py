import test_tools as tt
from hive_local_tools import run_for


@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_find_transaction(node, should_prepare):
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        wallet.create_account("gtg")
    head_block_number = node.api.wallet_bridge.get_dynamic_global_properties()["head_block_number"]
    operation = node.api.account_history.get_ops_in_block(block_num=head_block_number, include_reversible=True)

    assert len(operation["ops"]) > 0, f"Missing transactions in block {head_block_number}"

    transaction_id = operation["ops"][-1]["trx_id"]
    node.api.transaction_status.find_transaction(transaction_id=transaction_id)
