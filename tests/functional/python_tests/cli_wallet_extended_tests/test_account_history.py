def test_getting_empty_history(wallet):
    assert len(wallet.api.get_account_history("initminer", -1, 1)) == 0
