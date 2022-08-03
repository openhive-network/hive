import test_tools as tt

from ..local_tools import prepare_escrow, run_for


@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_get_escrow(prepared_node, should_prepare):
    if should_prepare:
        wallet = tt.Wallet(attach_to=prepared_node)
        prepare_escrow(wallet, sender='addicttolife')
    # Function since `HF14`. Not used in 5m block.
    prepared_node.api.condenser.get_escrow('addicttolife', 1)
