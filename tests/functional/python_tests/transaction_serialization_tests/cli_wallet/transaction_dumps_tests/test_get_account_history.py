def test_get_account_history(wallet):
    wallet.api.get_account_history('initminer', -1, 1)
