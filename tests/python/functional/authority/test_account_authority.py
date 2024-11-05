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
        assert not node.api.database.verify_account_authority(
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


@run_for("testnet")
def test_sign_owner_required_transaction_by_account_authority(
    node: tt.InitNode, initminer_wallet: tt.Wallet, bob: Account, carol: Account
) -> None:
    """
           ● owner -> carol (account_auth)
           │
    bob ── ● active
           │
           ● posting
    """
    bob.wallet.api.import_key(tt.PrivateKey(account_name=bob.name, secret="owner"))
    assert len(bob.wallet.api.list_keys()) == 1, "Bob's wallet has an incorrect number of imported keys. Expected 1"

    bob.wallet.api.update_account_auth_account(bob.name, "owner", carol.name, 1)
    assert (
        len(get_authority(node, bob.name, authority_level="owner").account_auths) == 1
    ), "Bob's owner-account_auths is not set"

    carol.wallet.api.import_key(tt.PrivateKey(account_name=carol.name, secret="active"))
    carol.wallet.api.import_key(tt.PrivateKey(account_name=carol.name, secret="owner"))
    assert len(carol.wallet.api.list_keys()) == 2, "Carol's wallet has an incorrect number of imported keys. Expected 2"

    # carol signing an `bob - owner type - operation` using carol - active authority
    carol.wallet.api.update_account(
        bob.name,
        "{}",
        tt.PublicKey("account", secret="new-owner-key"),
        tt.PublicKey("account", secret="active"),
        tt.PublicKey("account", secret="posting"),
        tt.PublicKey("account", secret="memo"),
    )


@run_for("testnet")
def test_signing_with_circular_account_authority(
    node: tt.InitNode, initminer_wallet: tt.Wallet, bob: Account, carol: Account
) -> None:
    """
     ■─────────────────────────────────────────────────────────────■
     │                                                             │
     │     ● owner -> carol (account_auth) ■────■      ● owner -> bob (account_auth)
     ↓     │                                    ↓      │
    bob ── ● active                           carol ── ● active
           │                                           │
           ● posting                                   ● posting
    """
    initminer_wallet.api.import_keys(
        [tt.PrivateKey(account_name=bob.name, secret="owner"), tt.PrivateKey(account_name=carol.name, secret="owner")]
    )

    bob.wallet.api.import_key(tt.PrivateKey(account_name=bob.name, secret="active"))
    assert len(bob.wallet.api.list_keys()) == 1, "Bob's wallet has an incorrect number of imported keys. Expected 1"

    initminer_wallet.api.update_account_auth_account(bob.name, "owner", carol.name, 1)
    assert (
        len(get_authority(node, bob.name, authority_level="owner").account_auths) == 1
    ), "Bob's owner-account_auths is not set"

    initminer_wallet.api.update_account_auth_account(carol.name, "owner", bob.name, 1)
    assert (
        len(get_authority(node, carol.name, authority_level="owner").account_auths) == 1
    ), "Carol's owner-account_auths is not set"

    # bob trying broadcast a self - owner type operation (by active authority)
    bob.wallet.api.use_authority("active", bob.name)
    decline_voting_rights = bob.wallet.api.decline_voting_rights(bob.name, True, broadcast=False)
    with pytest.raises(tt.exceptions.CommunicationError):
        bob.wallet.api.sign_transaction(decline_voting_rights)

    """
     ■─────────────────────────────────────────────────────────────■─────────────────────■
     │                                                             │                     │
     │     ● owner -> carol (account_auth) ■────■      ● owner -> bob (account_auth)     │
     ↓     │                                    ↓      │                                 │
    bob ── ● active                           carol ── ● active -> bob (account_auth) ───■
           │                                           │
           ● posting                                   ● posting
    """
    # bob as carol's owner account authority can't set carol's active authority to redirect to bob
    bob.wallet.api.use_authority("active", bob.name)
    with pytest.raises(tt.exceptions.CommunicationError):
        bob.wallet.api.update_account_auth_account(carol.name, "active", bob.name, 1)

    # bob can't sign owner type operation (use active bob authority)
    bob.wallet.api.use_authority("active", bob.name)
    with pytest.raises(tt.exceptions.CommunicationError):
        bob.wallet.api.sign_transaction(decline_voting_rights)

    carol.wallet.api.import_key(tt.PrivateKey(account_name=carol.name, secret="owner"))
    assert len(carol.wallet.api.list_keys()) == 1, "Carol's wallet has an incorrect number of imported keys. Expected 1"
    carol.wallet.api.use_authority("owner", carol.name)
    # carol can't sign owner type operation (use owner carol authority)
    with pytest.raises(tt.exceptions.CommunicationError):
        carol.wallet.api.sign_transaction(decline_voting_rights)


@run_for("testnet")
def test_automatic_transaction_signing_by_key_with_higher_authority_level(
    node: tt.InitNode, initminer_wallet: tt.Wallet, bob: Account
) -> None:
    """https://gitlab.syncad.com/hive/hive/-/issues/703"""
    bob.wallet.api.import_key(tt.PrivateKey(account_name=bob.name, secret="active"))

    bob.wallet.api.use_automatic_authority()

    post = bob.wallet.api.post_comment(bob.name, "test-permlink", "", "someone", "title", "body", "{}", broadcast=False)
    with pytest.raises(tt.exceptions.CommunicationError) as error:
        bob.wallet.api.sign_transaction(post)
    assert "missing required posting authority" in error.value.error
