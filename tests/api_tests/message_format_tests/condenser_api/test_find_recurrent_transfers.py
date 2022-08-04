import test_tools as tt

from ..local_tools import run_for


@run_for('testnet')
def test_find_recurrent_transfers(prepared_node, should_prepare):
    if should_prepare:
        wallet = tt.Wallet(attach_to=prepared_node)
        wallet.api.create_account('initminer', 'alice', '{}')
        wallet.api.recurrent_transfer('initminer', 'alice', tt.Asset.Test(10), '{}', 720, 12)
    prepared_node.api.condenser.find_recurrent_transfers('initminer')
