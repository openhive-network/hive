def test_account_creation(wallet):
    wallet.api.create_account('initminer', 'alice', '{}')
    assert wallet.api.list_accounts('a', 1) == ['alice']
