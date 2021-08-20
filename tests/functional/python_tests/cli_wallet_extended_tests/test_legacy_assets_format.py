# In future, legacy assets format (e.g. '100.000 TESTS') will be replaced with nai format.
# These tests are to ensure proper assets format (legacy) in wallet api. These tests are
# needed only temporarily. When Protocol Buffers will be implemented, then correct format
# (nai at this moment) will be enforced by them. This file should be removed then.

from test_tools import Asset


def test_format_in_list_my_accounts(wallet):
    response = wallet.api.create_account('initminer', 'alice', '{}')
    owner_key = response['result']['operations'][0][1]['owner']['key_auths'][0][0]

    response = wallet.api.list_my_accounts([owner_key])
    assert response['result'][0]['balance'] == Asset.Test(0)
    assert response['result'][0]['savings_balance'] == Asset.Test(0)


def test_format_in_get_account(wallet):
    wallet.api.create_account('initminer', 'alice', '{}')

    response = wallet.api.get_account('alice')
    assert response['result']['hbd_balance'] == Asset.Tbd(0)
