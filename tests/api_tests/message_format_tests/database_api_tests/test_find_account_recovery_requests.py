from ..local_tools import request_account_recovery


def test_find_account_recovery_requests(node, wallet):
    wallet.api.create_account('initminer', 'alice', '{}')
    request_account_recovery(wallet, 'alice')
    requests = node.api.database.find_account_recovery_requests(accounts=['alice'])['requests']
    assert len(requests) != 0
