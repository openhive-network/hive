import test_tools as tt

from ..local_tools import run_for


@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_list_accounts(prepared_node, should_prepare):
    if should_prepare:
        wallet = tt.Wallet(attach_to=prepared_node)
        wallet.api.create_account('initminer', 'alice', '{}')
    accounts = prepared_node.api.database.list_accounts(start='', limit=100, order='by_name')['accounts']
    assert len(accounts) != 0
