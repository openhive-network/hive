import test_tools as tt

from ..local_tools import run_for


@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_find_recurrent_transfers(node, should_prepare):
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        wallet.api.create_account('initminer', 'alice', '{}')
        wallet.api.recurrent_transfer('initminer', 'alice', tt.Asset.Test(10), '{}', 720, 12)
    node.api.condenser.find_recurrent_transfers('initminer')
