def test_getting_empty_history(wallet):
    response = wallet.api.get_account_history('initminer', -1, 1)
    assert len(response['result']) == 0
