from __future__ import annotations

import pytest

import test_tools as tt


@pytest.fixture(scope="module")
def prepared_node() -> tt.InitNode:
    """Module-scoped testnet node with blockchain state prepared for list API tests."""
    node = tt.InitNode()
    node.config.plugin.append("condenser_api")
    node.config.block_log_split = -1
    node.run(timeout=60.0)

    wallet = tt.Wallet(attach_to=node)

    # Create accounts with funds
    wallet.create_account("alice", hives=tt.Asset.Test(500), vests=tt.Asset.Test(500), hbds=tt.Asset.Tbd(500))
    wallet.create_account("bob", hives=tt.Asset.Test(500), vests=tt.Asset.Test(500), hbds=tt.Asset.Tbd(500))

    # Register alice as a witness
    wallet.api.update_witness(
        "alice",
        "http://url.html",
        tt.Account("alice").public_key,
        {"account_creation_fee": tt.Asset.Test(28), "maximum_block_size": 131072, "hbd_interest_rate": 1000},
    )

    # bob votes for alice as witness
    wallet.api.vote_for_witness("bob", "alice", True)

    # Vesting delegation: alice -> bob
    wallet.api.delegate_vesting_shares("alice", "bob", tt.Asset.Vest(10))

    # Limit order: alice sells HIVE for HBD
    wallet.api.create_order("alice", 431, tt.Asset.Test(10), tt.Asset.Tbd(1), False, 3600)

    # HBD conversion request
    wallet.api.convert_hbd("alice", tt.Asset.Tbd(5))

    # Collateralized conversion request
    wallet.api.convert_hive_with_collateral("alice", tt.Asset.Test(10))

    # Savings withdrawal
    wallet.api.transfer_to_savings("alice", "alice", tt.Asset.Test(50), "save")
    wallet.api.transfer_from_savings("alice", 100, "alice", tt.Asset.Test(10), "withdraw")

    # Proposal: alice posts, creates proposal, bob votes for it
    wallet.api.post_comment("alice", "test-post", "", "test", "Test Post", "body", "{}")
    wallet.api.create_proposal(
        "alice", "alice", "2031-01-01T00:00:00", "2031-06-01T00:00:00", tt.Asset.Tbd(10), "test proposal", "test-post"
    )
    wallet.api.update_proposal_votes("bob", [0], True)

    # Decline voting rights for bob
    wallet.api.decline_voting_rights("bob", True)

    # Change recovery account: alice changes recovery to bob
    wallet.api.change_recovery_account("alice", "bob")

    # Owner history: update alice's owner key to generate an owner_auth_history entry
    wallet.api.update_account(
        "alice",
        "{}",
        tt.Account("carol").public_key,
        tt.Account("alice").public_key,
        tt.Account("alice").public_key,
        tt.Account("alice").public_key,
    )

    # Withdraw vesting route: alice -> bob
    wallet.api.set_withdraw_vesting_route("alice", "bob", 50, True)

    # Wait for a block to confirm all transactions
    node.wait_number_of_blocks(2)

    return node
