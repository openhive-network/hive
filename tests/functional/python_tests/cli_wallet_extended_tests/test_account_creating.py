def test_account_creation(wallet):
    wallet.api.create_account('initminer', 'alice', '{}')

    response = wallet.api.list_accounts('a', 1)
    assert response['result'] == ['alice']
