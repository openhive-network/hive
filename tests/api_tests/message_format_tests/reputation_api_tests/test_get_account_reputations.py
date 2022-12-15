from ....local_tools import run_for


@run_for('testnet', 'mainnet_5m', 'live_mainnet')
def test_get_account_reputations(node):
    node.api.reputation.get_account_reputations(account_lower_bound="", limit=1000)
