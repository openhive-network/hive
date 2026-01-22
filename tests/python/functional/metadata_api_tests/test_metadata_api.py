from __future__ import annotations

import json

import pytest

import test_tools as tt


@pytest.fixture()
def node() -> tt.InitNode:
    node = tt.InitNode()
    node.config.plugin.append("metadata_api")
    node.run()
    return node


@pytest.fixture()
def wallet(node: tt.InitNode) -> tt.Wallet:
    return tt.Wallet(attach_to=node)


def test_get_account_metadata_basic(node: tt.InitNode, wallet: tt.Wallet) -> None:
    """Test basic get_account_metadata functionality."""
    wallet.create_account("alice", hives=tt.Asset.Test(100), vests=tt.Asset.Test(100))

    # Initially, metadata should be empty
    result = node.api.metadata.get_account_metadata(account="alice")
    assert result.json_metadata == ""
    assert result.posting_json_metadata == ""

    # Update account metadata
    wallet.api.update_account_meta("alice", '{"profile": {"name": "Alice"}}')

    # Verify metadata was updated
    result = node.api.metadata.get_account_metadata(account="alice")
    assert json.loads(result.json_metadata) == {"profile": {"name": "Alice"}}


def test_get_account_metadata_with_posting_metadata(node: tt.InitNode, wallet: tt.Wallet) -> None:
    """Test get_account_metadata with posting_json_metadata."""
    wallet.create_account("bob", hives=tt.Asset.Test(100), vests=tt.Asset.Test(100))

    # Use account_update2 to set both json_metadata and posting_json_metadata
    wallet.api.update_account(
        "bob",
        '{"type": "user"}',  # json_metadata
        wallet.api.get_account("bob")["owner"]["key_auths"][0][0],  # owner key
        wallet.api.get_account("bob")["active"]["key_auths"][0][0],  # active key
        wallet.api.get_account("bob")["posting"]["key_auths"][0][0],  # posting key
        wallet.api.get_account("bob")["memo_key"],  # memo key
    )

    result = node.api.metadata.get_account_metadata(account="bob")
    assert json.loads(result.json_metadata) == {"type": "user"}


def test_get_account_metadata_nonexistent_account(node: tt.InitNode) -> None:
    """Test get_account_metadata with non-existent account."""
    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.metadata.get_account_metadata(account="nonexistent")


def test_find_account_metadata_multiple_accounts(node: tt.InitNode, wallet: tt.Wallet) -> None:
    """Test find_account_metadata with multiple accounts."""
    # Create accounts
    wallet.create_account("user1", hives=tt.Asset.Test(100), vests=tt.Asset.Test(100))
    wallet.create_account("user2", hives=tt.Asset.Test(100), vests=tt.Asset.Test(100))
    wallet.create_account("user3", hives=tt.Asset.Test(100), vests=tt.Asset.Test(100))

    # Update metadata for some accounts
    wallet.api.update_account_meta("user1", '{"role": "admin"}')
    wallet.api.update_account_meta("user2", '{"role": "moderator"}')

    # Query multiple accounts at once
    result = node.api.metadata.find_account_metadata(accounts=["user1", "user2", "user3"])

    assert len(result.metadata) == 3

    # Check user1
    user1_meta = next(m for m in result.metadata if m.account == "user1")
    assert json.loads(user1_meta.json_metadata) == {"role": "admin"}

    # Check user2
    user2_meta = next(m for m in result.metadata if m.account == "user2")
    assert json.loads(user2_meta.json_metadata) == {"role": "moderator"}

    # Check user3 (no metadata set)
    user3_meta = next(m for m in result.metadata if m.account == "user3")
    assert user3_meta.json_metadata == ""


def test_find_account_metadata_with_nonexistent_account(node: tt.InitNode, wallet: tt.Wallet) -> None:
    """Test find_account_metadata with mix of existing and non-existing accounts."""
    wallet.create_account("existing", hives=tt.Asset.Test(100), vests=tt.Asset.Test(100))

    # Query with mix of existing and non-existing accounts
    result = node.api.metadata.find_account_metadata(accounts=["existing", "nonexistent"])

    # Only existing account should be in results
    assert len(result.metadata) == 1
    assert result.metadata[0].account == "existing"


def test_metadata_persists_across_operations(node: tt.InitNode, wallet: tt.Wallet) -> None:
    """Test that metadata persists across multiple account operations."""
    wallet.create_account("charlie", hives=tt.Asset.Test(100), vests=tt.Asset.Test(100))

    # Set initial metadata
    wallet.api.update_account_meta("charlie", '{"version": 1}')

    # Do some other operations
    wallet.api.transfer("initminer", "charlie", tt.Asset.Test(10), "test transfer")

    # Verify metadata is still there
    result = node.api.metadata.get_account_metadata(account="charlie")
    assert json.loads(result.json_metadata) == {"version": 1}

    # Update metadata again
    wallet.api.update_account_meta("charlie", '{"version": 2}')

    result = node.api.metadata.get_account_metadata(account="charlie")
    assert json.loads(result.json_metadata) == {"version": 2}


def test_wallet_get_account_metadata_integration(wallet: tt.Wallet) -> None:
    """Test that wallet.api.get_account_metadata works correctly."""
    wallet.create_account("dave", hives=tt.Asset.Test(100), vests=tt.Asset.Test(100))
    wallet.api.update_account_meta("dave", '{"app": "test"}')

    # Test via wallet API
    result = wallet.api.get_account_metadata("dave")
    assert json.loads(result["json_metadata"]) == {"app": "test"}


def test_wallet_find_account_metadata_integration(wallet: tt.Wallet) -> None:
    """Test that wallet.api.find_account_metadata works correctly."""
    wallet.create_account("eve", hives=tt.Asset.Test(100), vests=tt.Asset.Test(100))
    wallet.create_account("frank", hives=tt.Asset.Test(100), vests=tt.Asset.Test(100))
    wallet.api.update_account_meta("eve", '{"role": "tester"}')
    wallet.api.update_account_meta("frank", '{"role": "developer"}')

    # Test via wallet API
    result = wallet.api.find_account_metadata(["eve", "frank"])
    assert len(result["metadata"]) == 2

    eve_meta = next(m for m in result["metadata"] if m["account"] == "eve")
    assert json.loads(eve_meta["json_metadata"]) == {"role": "tester"}

    frank_meta = next(m for m in result["metadata"] if m["account"] == "frank")
    assert json.loads(frank_meta["json_metadata"]) == {"role": "developer"}
