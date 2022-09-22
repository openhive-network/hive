def test_list_accounts(node, wallet):
    wallet.api.create_account('initminer', 'alice', '{}')
    accounts = node.api.database.list_accounts(start='', limit=100, order='by_name')['accounts']
    assert len(accounts) != 0
