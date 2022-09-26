import test_tools as tt

from ..local_tools import run_for


@run_for('testnet')
def test_get_required_signatures_in_testnet(prepared_node):
    wallet = tt.Wallet(attach_to=prepared_node, additional_arguments=['--transaction-serialization=hf26'])
    transaction = wallet.api.create_account('initminer', 'alice', '{}')
    keys = prepared_node.api.database.get_required_signatures(trx=transaction, available_keys=[tt.Account('initminer').public_key])['keys']
    assert len(keys) != 0


@run_for('mainnet_5m', 'mainnet_64m')
def test_get_required_signatures_in_mainnet(prepared_node):
    block = prepared_node.api.wallet_bridge.get_block(3652254)['block']
    account = prepared_node.api.wallet_bridge.get_accounts(['steem'])[0]
    transaction = block['transactions'][3]
    active_key = account['active']['key_auths'][0][0]
    keys = prepared_node.api.database.get_required_signatures(trx=transaction, available_keys=[active_key])['keys']
    assert len(keys) != 0
