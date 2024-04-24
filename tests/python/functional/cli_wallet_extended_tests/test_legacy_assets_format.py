# In future, legacy assets format (e.g. '100.000 TESTS') will be replaced with nai format.
# These tests are to ensure proper assets format (legacy) in wallet api. These tests are
# needed only temporarily. When Protocol Buffers will be implemented, then correct format
# (nai at this moment) will be enforced by them. This file should be removed then.
from __future__ import annotations

import test_tools as tt


def test_format_in_list_my_accounts(old_wallet: tt.OldWallet) -> None:
    response = old_wallet.api.create_account("initminer", "alice", "{}")

    response = old_wallet.api.list_my_accounts()
    assert tt.Asset.from_legacy(response[0]["balance"]) == tt.Asset.Test(0)
    assert tt.Asset.from_legacy(response[0]["savings_balance"]) == tt.Asset.Test(0)


def test_format_in_get_account(old_wallet: tt.OldWallet) -> None:
    old_wallet.api.create_account("initminer", "alice", "{}")

    response = old_wallet.api.get_account("alice")
    assert tt.Asset.from_legacy(response["hbd_balance"]) == tt.Asset.Tbd(0)
