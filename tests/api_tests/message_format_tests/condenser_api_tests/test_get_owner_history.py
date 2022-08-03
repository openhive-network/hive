import test_tools as tt

from ..local_tools import run_for


@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_get_owner_history(prepared_node, should_prepare):
    if should_prepare:
        wallet = tt.Wallet(attach_to=prepared_node)
        # abramov is a real user existing on a 5m node. For test standardization, it was also created in testnet.
        wallet.api.create_account('initminer', 'abramov', '{}')
        wallet.api.transfer_to_vesting('initminer', 'abramov', tt.Asset.Test(500))
        key = tt.Account('abramov', secret='other_than_previous').public_key
        wallet.api.update_account('abramov', '{}', key, key, key, key)
    prepared_node.api.condenser.get_owner_history('abramov')
