import test_tools as tt

from hive_local_tools import run_for


@run_for('testnet')
def test_get_required_signatures_in_testnet(node):
    wallet = tt.Wallet(attach_to=node, additional_arguments=['--transaction-serialization=hf26'])
    transaction = wallet.api.create_account('initminer', 'alice', '{}')
    keys = node.api.database.get_required_signatures(trx=transaction, available_keys=[tt.Account('initminer').public_key])['keys']
    assert len(keys) != 0


@run_for('mainnet_5m', 'live_mainnet')
def test_get_required_signatures_in_mainnet(node):
    block = node.api.wallet_bridge.get_block(3652254)['block']
    transaction = block['transactions'][3]

    account = node.api.wallet_bridge.get_accounts(['steem'])[0]
    active_key = account['active']['key_auths'][0][0]

    keys = node.api.database.get_required_signatures(trx=transaction, available_keys=[active_key])['keys']
    assert len(keys) != 0
