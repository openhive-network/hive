from __future__ import annotations

import pytest

import test_tools as tt

from .utilities import get_key


@pytest.mark.skip(reason="Authorization bug")
def test_recovery(wallet: tt.Wallet) -> None:
    wallet.api.create_account("initminer", "alice", "{}")

    wallet.api.transfer_to_vesting("initminer", "alice", tt.Asset.Test(100))

    wallet.api.transfer("initminer", "alice", tt.Asset.Test(500), "banana")

    wallet.api.create_account("alice", "bob", "{}")

    wallet.api.transfer_to_vesting("alice", "bob", tt.Asset.Test(40))

    wallet.api.transfer("initminer", "bob", tt.Asset.Test(333), "kiwi-banana")

    response = wallet.api.change_recovery_account("alice", "bob")

    _ops = response.operations
    assert len(_ops) == 1
    assert _ops[0].type == "change_recovery_account_operation"
    assert _ops[0].value.account_to_recover == "alice"
    assert _ops[0].value.new_recovery_account == "bob"

    alice_owner_key = get_key("owner", wallet.api.get_account("alice"))
    bob_owner_key = get_key("owner", wallet.api.get_account("bob"))

    authority = {"weight_threshold": 1, "account_auths": [], "key_auths": [[alice_owner_key, 1]]}

    response = wallet.api.request_account_recovery("alice", "bob", authority)

    _ops = response.operations
    assert _ops[0].type == "request_account_recovery_operation"
    assert _ops[0].value.recovery_account == "alice"
    assert _ops[0].value.account_to_recover == "bob"

    wallet.api.update_account_auth_key("bob", "owner", bob_owner_key, 3)

    recent_authority = {"weight_threshold": 1, "account_auths": [], "key_auths": [[bob_owner_key, 1]]}
    new_authority = {"weight_threshold": 1, "account_auths": [], "key_auths": [[alice_owner_key, 1]]}

    response = wallet.api.recover_account("bob", recent_authority, new_authority)  # hasz8

    _ops = response.operations
    assert _ops[0].type == "recover_account_operation"
    assert _ops[0].value.account_to_recover == "bob"
