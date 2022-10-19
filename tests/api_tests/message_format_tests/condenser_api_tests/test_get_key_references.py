import test_tools as tt

from ..local_tools import run_for


@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_get_key_references(node, should_prepare):
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        wallet.api.create_account('initminer', 'gtg', '{}')
    account_public_key = node.api.condenser.get_accounts(['gtg'])[0]['active']['key_auths'][0][0]
    node.api.condenser.get_key_references([account_public_key])
