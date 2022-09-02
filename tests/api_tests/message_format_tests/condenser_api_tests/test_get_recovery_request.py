from ..local_tools import run_for, request_account_recovery


# This test is not performed on 5 million block log because of lack of accounts with recovery requests within it. In
# case of most recent blocklog (current_blocklog) there is a lot of recovery requests, but they are changed after every
# remote node update. See the readme.md file in this directory for further explanation.
@run_for('testnet')
def test_get_recovery_request(prepared_node, wallet):
    wallet.api.create_account('initminer', 'alice', '{}')
    request_account_recovery(wallet, 'alice')
    requests = prepared_node.api.condenser.get_recovery_request('alice')
    assert len(requests) != 0
