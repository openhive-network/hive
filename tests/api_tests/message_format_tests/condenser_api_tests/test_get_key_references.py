import test_tools as tt

from ..local_tools import run_for


@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_get_key_references(prepared_node, should_prepare):
    if should_prepare:
        wallet = tt.Wallet(attach_to=prepared_node)
        wallet.api.create_account('initminer', 'gtg', '{}')
    account_public_key = prepared_node.api.condenser.get_accounts(['gtg'])[0]['active']['key_auths'][0][0]
    prepared_node.api.condenser.get_key_references([account_public_key])
