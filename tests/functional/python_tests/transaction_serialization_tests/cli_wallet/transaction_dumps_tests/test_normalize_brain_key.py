def test_normalize_brain_key(wallet):
    wallet.api.normalize_brain_key('     mango Apple CHERRY ') == 'MANGO APPLE CHERRY'

    #NIE DZIAŁA!!!!