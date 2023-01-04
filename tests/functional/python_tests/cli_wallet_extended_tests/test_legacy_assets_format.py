# In future, legacy assets format (e.g. '100.000 TESTS') will be replaced with nai format.
# These tests are to ensure proper assets format (legacy) in wallet api. These tests are
# needed only temporarily. When Protocol Buffers will be implemented, then correct format
# (nai at this moment) will be enforced by them. This file should be removed then.

import test_tools as tt


def test_format_in_list_my_accounts(wallet):
    response = wallet.api.create_account("initminer", "alice", "{}")

    response = wallet.api.list_my_accounts()
    assert response[0]["balance"] == tt.Asset.Test(0)
    assert response[0]["savings_balance"] == tt.Asset.Test(0)


def test_format_in_get_account(wallet):
    wallet.api.create_account("initminer", "alice", "{}")

    response = wallet.api.get_account("alice")
    assert response["hbd_balance"] == tt.Asset.Tbd(0)
