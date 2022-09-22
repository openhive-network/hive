def test_find_accounts(node, wallet):
    wallet.api.create_account('initminer', 'alice', '{}')
    accounts = node.api.database.find_accounts(accounts=['alice'])['accounts']
    assert len(accounts) != 0
