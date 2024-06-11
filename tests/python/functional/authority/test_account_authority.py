from __future__ import annotations

import pytest

import test_tools as tt
from hive_local_tools import run_for


@run_for("testnet")
@pytest.mark.parametrize("authority", ["posting", "active", "owner"])
def test_posting_account_authority(node: tt.InitNode, authority, alice, bob) -> None:
    alice.wallet.api.update_account_auth_account(alice.name, authority, bob.name, 1)
    assert node.api.database.find_accounts(accounts=[alice.name]).accounts[0][authority].account_auths[0][0] == bob.name

    bob.wallet.api.import_key(tt.PrivateKey(bob.name, secret=authority))
    bob.wallet.api.use_authority(authority, bob.name)

    if authority == "posting":
        bob.wallet.api.post_comment(alice.name, "test-permlink", "", "someone0", "test-title", "body", "{}")
    else:
        with pytest.raises(tt.exceptions.CommunicationError) as exception:
            bob.wallet.api.post_comment(alice.name, "test-permlink", "", "someone0", "test-title", "body", "{}")
        assert "missing required posting authority" in exception.value.error


@run_for("testnet")
@pytest.mark.parametrize("authority", ["posting", "active", "owner"])
def test_active_account_authority(node: tt.InitNode, authority, alice, bob) -> None:
    alice.wallet.api.update_account_auth_account(alice.name, authority, bob.name, 1)
    assert node.api.database.find_accounts(accounts=[alice.name]).accounts[0][authority].account_auths[0][0] == bob.name

    bob.wallet.api.import_key(tt.PrivateKey(bob.name, secret=authority))
    bob.wallet.api.use_authority(authority, bob.name)

    if authority == "active":
        bob.wallet.api.transfer("alice", "initminer", tt.Asset.Test(10), "bob signed alice transfer.")
    else:
        with pytest.raises(tt.exceptions.CommunicationError) as exception:
            bob.wallet.api.transfer("alice", "initminer", tt.Asset.Test(10), "bob signed alice transfer.")
        assert "missing required active authority" in exception.value.error


@run_for("testnet")
@pytest.mark.parametrize("authority", ["posting", "active", "owner"])
def test_owner_account_authority(node: tt.InitNode, authority, alice, bob) -> None:
    alice.wallet.api.update_account_auth_account(alice.name, authority, bob.name, 1)
    assert node.api.database.find_accounts(accounts=[alice.name]).accounts[0][authority].account_auths[0][0] == bob.name

    bob.wallet.api.import_key(tt.PrivateKey(bob.name, secret=authority))

    if authority == "owner":
        # fixme: ¿?
        bob.wallet.api.import_key(tt.PrivateKey(bob.name, secret="active"))
        bob.wallet.api.use_authority("active", bob.name)
        #

        bob.wallet.api.update_account(
            alice.name,
            "{}",
            owner=tt.Account("alice", secret="new-owner").public_key,
            posting=tt.Account("alice", secret="posting").public_key,
            active=tt.Account("alice", secret="active").public_key,
            memo=tt.Account("alice", secret="memo").public_key,
        )
    else:
        with pytest.raises(tt.exceptions.CommunicationError) as exception:
            bob.wallet.api.update_account(
                alice.name,
                "{}",
                owner=tt.Account("alice", secret="new-owner").public_key,
                posting=tt.Account("alice", secret="posting").public_key,
                active=tt.Account("alice", secret="active").public_key,
                memo=tt.Account("alice", secret="memo").public_key,
            )
        assert "missing required owner authority" in exception.value.error
