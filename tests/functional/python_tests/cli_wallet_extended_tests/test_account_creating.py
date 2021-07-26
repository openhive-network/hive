def test_account_creation(wallet):
    wallet.api.create_account('initminer', 'alice', '{}')

    response = wallet.api.list_accounts('a', 1)
    assert response['result'] == ['alice']


def test_account_creation_and_much_more(wallet):
    #**************************************************************
    response = wallet.api.get_account_history('initminer', 2, 2)
    #this call has a custom formatter so typical JSON is inaccessible
    assert len(response['result']) == 0
