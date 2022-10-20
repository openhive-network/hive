import test_tools as tt

from ..local_tools import run_for


@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_get_account_history(prepared_node, should_prepare):
    if should_prepare:
        wallet = tt.Wallet(attach_to=prepared_node)
        wallet.create_account('alice', hbds=tt.Asset.Tbd(100), vests=tt.Asset.Test(100))

        # Wait until block containing above transaction will become irreversible.
        prepared_node.wait_number_of_blocks(21)

    prepared_node.api.condenser.get_account_history('alice', -1, 10)
