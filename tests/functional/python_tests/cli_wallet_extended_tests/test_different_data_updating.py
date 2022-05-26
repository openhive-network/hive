from test_tools import Account, logger, World, Asset
from .utilities import check_keys
import json

def test_update(wallet):
    wallet.api.create_account('initminer', 'alice', '{}')

    wallet.api.transfer_to_vesting('initminer', 'alice', Asset.Test(500))

    wallet.api.update_account_auth_account('alice', 'posting', 'initminer', 2)

    response = wallet.api.get_account('alice')

    _posting = response['posting']
    _account_auths = _posting['account_auths']
    assert len(_account_auths) == 1
    __account_auths = _account_auths[0]
    assert len(__account_auths) == 2
    assert __account_auths[0] == 'initminer'
    assert __account_auths[1] == 2

    wallet.api.update_account_auth_key('alice', 'posting', 'TST8ViK3T9FHbbtQs9Mo5odBM6tSmtFEVCvjEDKNPqKe9U1bJs53f', 3)

    response = wallet.api.get_account('alice')

    _posting = response['posting']
    _key_auths = _posting['key_auths']
    assert len(_key_auths) == 2
    __key_auths = _key_auths[1]
    assert len(__key_auths) == 2
    __account_auths[0] == 'TST8ViK3T9FHbbtQs9Mo5odBM6tSmtFEVCvjEDKNPqKe9U1bJs53f'
    __account_auths[1] == 3

    wallet.api.update_account_auth_threshold('alice', 'posting', 4)

    response = wallet.api.get_account('alice')

    _posting = response['posting']
    _key_auths = _posting['key_auths']
    assert len(_key_auths) == 2
    __key_auths = _key_auths[1]
    assert len(__key_auths) == 2
    __account_auths[1] == 4

    wallet.api.update_account_memo_key('alice', 'TST84oS1GW3yb9QaRaGztrqH5cHqeFFyLgLGSK4FoLEoDFBJqCnSJ')

    response = wallet.api.get_account('alice')

    assert response['memo_key'] == 'TST84oS1GW3yb9QaRaGztrqH5cHqeFFyLgLGSK4FoLEoDFBJqCnSJ'

    wallet.api.update_account_meta('alice', '{ "test" : 4 }')

    assert json.loads(wallet.api.get_account('alice')['json_metadata']) == { "test" : 4 }

    key = 'TST8grZpsMPnH7sxbMVZHWEu1D26F3GwLW1fYnZEuwzT4Rtd57AER'

    wallet.api.update_account('alice', '{}', key, key, key, key)

    check_keys( wallet.api.get_account('alice'), key, key, key, key )

    _result = wallet.api.get_owner_history('alice')
    assert len(_result) == 1
    assert 'account' in _result[0]
    assert _result[0]['account'] == 'alice'
