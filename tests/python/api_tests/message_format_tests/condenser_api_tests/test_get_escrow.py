import test_tools as tt
from hive_local_tools import run_for
from hive_local_tools.api.message_format import prepare_escrow


@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_get_escrow(node, should_prepare):
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        prepare_escrow(wallet, sender="addicttolife")
    # Function since `HF14`. Not used in 5m block.
    node.api.condenser.get_escrow("addicttolife", 1)
