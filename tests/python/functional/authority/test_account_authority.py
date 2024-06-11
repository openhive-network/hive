from __future__ import annotations

from typing import TYPE_CHECKING

import pytest

import test_tools as tt
from hive_local_tools import run_for
from hive_local_tools.functional.python import get_authority

if TYPE_CHECKING:
    from hive_local_tools.functional.python.operation import Account


@run_for("testnet")
@pytest.mark.parametrize("authority_level", ["posting", "active", "owner"])
def test_posting_account_authority(node: tt.InitNode, authority_level: str, alice: Account, bob: Account) -> None:
    alice.wallet.api.update_account_auth_account(alice.name, authority_level, bob.name, 1)
    assert (bob.name, 1) in get_authority(
        node, alice.name, authority_level
    ).account_auths, f"Account {bob.name} was not set as account_auths for the account {alice.name}."

    bob.wallet.api.import_key(tt.PrivateKey(bob.name, secret=authority_level))
    assert len(bob.wallet.api.list_keys()) == 1, "Bob's wallet has an incorrect number of keys. Expected 1"
    bob.wallet.api.use_authority(authority_level, bob.name)

    is_bob_authorized_to_sign_alice_transaction = node.api.database.verify_account_authority(
        account=alice.name, signers=[tt.PublicKey(bob.name, secret=authority_level)], level="posting"
    ).valid

    if authority_level == "posting":
        assert is_bob_authorized_to_sign_alice_transaction
        bob.wallet.api.post_comment(alice.name, "test-permlink", "", "someone0", "test-title", "body", "{}")
    else:
        assert not is_bob_authorized_to_sign_alice_transaction
        with pytest.raises(tt.exceptions.CommunicationError) as exception:
            bob.wallet.api.post_comment(alice.name, "test-permlink", "", "someone0", "test-title", "body", "{}")
        assert "missing required posting authority" in exception.value.error


@run_for("testnet")
@pytest.mark.parametrize("authority_level", ["posting", "active", "owner"])
def test_active_account_authority(node: tt.InitNode, authority_level: str, alice: Account, bob: Account) -> None:
    alice.wallet.api.update_account_auth_account(alice.name, authority_level, bob.name, 1)
    assert (bob.name, 1) in get_authority(
        node, alice.name, authority_level
    ).account_auths, f"Account {bob.name} was not set as account_auths for the account {alice.name}."

    bob.wallet.api.import_key(tt.PrivateKey(bob.name, secret=authority_level))
    assert len(bob.wallet.api.list_keys()) == 1, "Bob's wallet has an incorrect number of keys. Expected 1"
    bob.wallet.api.use_authority(authority_level, bob.name)

    is_bob_authorized_to_sign_alice_transaction = node.api.database.verify_account_authority(
        account=alice.name, signers=[tt.PublicKey(bob.name, secret=authority_level)], level="active"
    ).valid

    if authority_level == "active":
        assert is_bob_authorized_to_sign_alice_transaction
        bob.wallet.api.transfer("alice", "initminer", tt.Asset.Test(10), "bob signed alice transfer.")
    else:
        assert not is_bob_authorized_to_sign_alice_transaction
        with pytest.raises(tt.exceptions.CommunicationError) as exception:
            bob.wallet.api.transfer("alice", "initminer", tt.Asset.Test(10), "bob signed alice transfer.")
        assert "missing required active authority" in exception.value.error


@run_for("testnet")
@pytest.mark.parametrize("authority_level", ["posting", "active", "owner"])
def test_owner_account_authority(node: tt.InitNode, authority_level: str, alice: Account, bob: Account) -> None:
    alice.wallet.api.update_account_auth_account(alice.name, authority_level, bob.name, 1)
    assert (bob.name, 1) in get_authority(
        node, alice.name, authority_level
    ).account_auths, f"Account {bob.name} was not set as account_auths for the account {alice.name}."

    if authority_level == "owner":
        assert node.api.database.verify_account_authority(
            account=alice.name, signers=[tt.PublicKey(bob.name, secret="active")], level="active"
        ).valid

        bob.wallet.api.import_key(tt.PrivateKey(bob.name, secret="active"))
        assert len(bob.wallet.api.list_keys()) == 1, "Bob's wallet has an incorrect number of keys. Expected 1"
        bob.wallet.api.use_authority("active", bob.name)

        bob.wallet.api.update_account(
            alice.name,
            "{}",
            owner=tt.Account("alice", secret="new-owner").public_key,
            posting=tt.Account("alice", secret="posting").public_key,
            active=tt.Account("alice", secret="active").public_key,
            memo=tt.Account("alice", secret="memo").public_key,
        )
    else:
        assert not node.api.database.verify_account_authority(
            account=alice.name, signers=[tt.PublicKey(bob.name, secret=authority_level)], level="owner"
        ).valid

        bob.wallet.api.import_key(tt.PrivateKey(bob.name, secret=authority_level))
        assert len(bob.wallet.api.list_keys()) == 1, "Bob's wallet has an incorrect number of keys. Expected 1"

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
