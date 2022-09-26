import test_tools as tt

from ..local_tools import request_account_recovery, run_for


# This test is not performed on 5 million block log because of lack of accounts with recovery requests within it. In
# case of most recent blocklog (current_blocklog) there is a lot of recovery requests, but they are changed after every
# remote node update. See the readme.md file in this directory for further explanation.
@run_for('testnet')
def test_list_account_recovery_requests(prepared_node, should_prepare):
    wallet = tt.Wallet(attach_to=prepared_node)
    wallet.api.create_account('initminer', 'alice', '{}')
    request_account_recovery(wallet, 'alice')
    requests = prepared_node.api.database.list_account_recovery_requests(start='', limit=100, order='by_account')['requests']
    assert len(requests) != 0
