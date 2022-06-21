def test_create_account_with_keys(wallet):
    key = 'TST8grZpsMPnH7sxbMVZHWEu1D26F3GwLW1fYnZEuwzT4Rtd57AER'
    wallet.api.create_account_with_keys('initminer', 'alice1', '{}', key, key, key, key)
