import test_tools as tt

from hive_local_tools import run_for


@run_for('testnet', 'mainnet_5m', 'live_mainnet', enable_plugins=['account_history_api'])
def test_get_account_history(node, should_prepare):
    preparation_for_testnet_node(node, should_prepare)
    node.api.condenser.get_account_history('alice', -1, 10, 5, 10)


@run_for('testnet', 'mainnet_5m', 'live_mainnet', enable_plugins=['account_history_api'])
def test_get_account_history_with_default_fifth_argument(node, should_prepare):
    preparation_for_testnet_node(node, should_prepare)
    node.api.condenser.get_account_history('alice', -1, 10, 5)


@run_for('testnet', 'mainnet_5m', 'live_mainnet', enable_plugins=['account_history_api'])
def test_get_account_history_with_default_fourth_and_fifth_argument(node, should_prepare):
    preparation_for_testnet_node(node, should_prepare)
    node.api.condenser.get_account_history('alice', -1, 10)


def preparation_for_testnet_node(node, should_prepare):
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        wallet.create_account('alice', hbds=tt.Asset.Tbd(100), vests=tt.Asset.Test(100))
        node.wait_for_irreversible_block()
