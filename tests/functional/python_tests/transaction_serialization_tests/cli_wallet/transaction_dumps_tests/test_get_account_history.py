def test_get_account_history(wallet):   # NIE DZIAŁA
    wallet.api.get_account_history('initminer', -1, 1)
