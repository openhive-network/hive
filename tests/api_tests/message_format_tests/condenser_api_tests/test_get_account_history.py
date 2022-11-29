import test_tools as tt

from ....local_tools import run_for


@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_get_account_history(node, should_prepare):
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        wallet.create_account('alice', hbds=tt.Asset.Tbd(100), vests=tt.Asset.Test(100))
        # Wait until block containing above transaction will become irreversible.
        node.api.debug_node.debug_generate_blocks(debug_key=tt.Account('initminer').private_key, count=21,
                                                           skip=0, miss_blocks=0, edit_if_needed=True)
    node.api.condenser.get_account_history('alice', -1, 10)
