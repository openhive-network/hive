def test_normalize_brain_key(wallet):    #NIE DZIAŁA!!!!
    wallet.api.normalize_brain_key('     mango Apple CHERRY ') == 'MANGO APPLE CHERRY'

