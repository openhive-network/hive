from hive_local_tools import run_for


@run_for('testnet', 'mainnet_5m', 'live_mainnet')
def test_get_account_reputations(node):
    node.api.condenser.get_account_reputations('', 100)


@run_for('testnet', 'mainnet_5m', 'live_mainnet')
def test_get_account_reputations_with_default_second_argument(node):
    node.api.condenser.get_account_reputations('')
