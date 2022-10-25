from ..local_tools import run_for


@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_get_ops_in_block(node, should_prepare):
    if should_prepare:
        # Wait until block containing above transaction will become irreversible.
        node.wait_number_of_blocks(21)
    node.api.condenser.get_ops_in_block(1, False)
