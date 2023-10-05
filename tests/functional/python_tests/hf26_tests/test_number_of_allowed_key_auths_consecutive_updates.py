import pytest

import test_tools as tt


def test_multiple_auth_key_updates_in_hf25(wallet_hf25):
    wallet_hf25.create_account("alice", vests=tt.Asset.Test(100))

    owner_keys = [tt.PublicKey(account) for account in ["bob", "carol"]]

    wallet_hf25.api.update_account_auth_key("alice", "owner", owner_keys[0], 3)

    # In HF25 it was allowed to update authorization key only once per hour, so second try was rejected.
    with pytest.raises(tt.exceptions.CommunicationError):
        wallet_hf25.api.update_account_auth_key("alice", "owner", owner_keys[1], 3)


def test_multiple_auth_key_updates_in_hf26(wallet_hf26):
    wallet_hf26.create_account("alice", vests=tt.Asset.Test(100))

    owner_keys = [tt.PublicKey(account) for account in ["bob", "carol", "dan"]]

    # In HF26 it was allowed to update authorization key twice per hour...
    for owner_key in owner_keys[:-1]:
        wallet_hf26.api.update_account_auth_key("alice", "owner", owner_key, 3)

    # ...so third consecutive try should be rejected.
    with pytest.raises(tt.exceptions.CommunicationError):
        wallet_hf26.api.update_account_auth_key("alice", "owner", owner_keys[-1], 3)
