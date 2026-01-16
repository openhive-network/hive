from __future__ import annotations

import pytest
import test_tools as tt
from beekeepy.exceptions import ErrorInResponseError


@pytest.fixture
def node() -> tt.InitNode:
    node = tt.InitNode()
    node.run()
    return node


@pytest.fixture
def wallet(node: tt.InitNode) -> tt.Wallet:
    return tt.Wallet(attach_to=node)


def test_in_single_transaction(wallet: tt.Wallet) -> None:
    wallet.create_account("alice", hives=tt.Asset.Test(100), vests=tt.Asset.Test(100))
    with wallet.in_single_transaction():
        wallet.api.post_comment(
            "initminer", "test-permlink", "", "test-parent-permlink", "test-title", "test-body", "{}"
        )
        wallet.api.post_comment(
            "alice", "test-permlink1", "", "test-parent-permlink1", "test-title1", "test-body1", "{}"
        )


def test_create_account(wallet: tt.Wallet) -> None:
    wallet.api.create_account("initminer", "alice", "{}")


def test_post_comment(wallet: tt.Wallet) -> None:
    wallet.api.create_account("initminer", "alice", "{}")
    wallet.api.post_comment("initminer", "test-permlink", "", "test-parent-permlink", "test-title", "test-body", "{}")


def test_create_account_with_keys(wallet: tt.Wallet) -> None:
    alice = tt.Account("alice")
    wallet.api.create_account_with_keys(
        "initminer",
        alice.name,
        "{}",
        alice.public_key,
        alice.public_key,
        alice.public_key,
        alice.public_key,
        broadcast=False,
    )


def test_create_account_delegated(wallet: tt.Wallet) -> None:
    try:
        wallet.api.create_account_delegated("initminer", tt.Asset.Test(3), tt.Asset.Vest(6.123456), "alicex", "{}")
    except ErrorInResponseError as e:
        message = str(e)
        found = message.find("Account creation with delegation is deprecated as of Hardfork 20")
        assert found != -1


def test_create_account_with_keys_delegated(wallet: tt.Wallet) -> None:
    alice = tt.Account("alice")
    try:
        wallet.api.create_account_with_keys_delegated(
            "initminer",
            tt.Asset.Test(3),
            tt.Asset.Vest(6.123456),
            "alicex",
            "{}",
            alice.public_key,
            alice.public_key,
            alice.public_key,
            alice.public_key,
        )
    except ErrorInResponseError as e:
        message = str(e)
        found = message.find("Account creation with delegation is deprecated as of Hardfork 20")
        assert found != -1


def test_create_funded_account_with_keys(wallet: tt.Wallet) -> None:
    account = tt.Account("alice")
    key = account.public_key
    wallet.api.create_funded_account_with_keys(
        "initminer", account.name, tt.Asset.Test(0), "memo", "{}", key, key, key, key
    )


def test_create_order(wallet: tt.Wallet) -> None:
    wallet.create_account("alice", hives=tt.Asset.Test(100), vests=tt.Asset.Test(100))
    wallet.api.create_order("alice", 1000, tt.Asset.Test(100), tt.Asset.Tbd(100), False, 1000)


def test_cancel_order(wallet: tt.Wallet) -> None:
    wallet.create_account("alice", hives=tt.Asset.Test(100), vests=tt.Asset.Test(100))
    wallet.api.create_order("initminer", 1000, tt.Asset.Test(100), tt.Asset.Tbd(100), False, 1000)
    wallet.api.cancel_order("initminer", 1000)


def test_transfer(wallet: tt.Wallet) -> None:
    wallet.api.create_account("initminer", "alice", "{}")
    wallet.api.transfer("initminer", "alice", tt.Asset.Test(500), "banana")


def test_transfer_to_vesting(wallet: tt.Wallet) -> None:
    wallet.api.create_account("initminer", "alice", "{}")
    wallet.api.transfer_to_vesting("initminer", "alice", tt.Asset.Hive(500))


def test_create_proposal(wallet: tt.Wallet) -> None:
    wallet.api.post_comment("initminer", "permlink", "", "parent-permlink", "title", "body", "{}")
    wallet.api.create_proposal(
        "initminer",
        "initminer",
        tt.Time.from_now(weeks=16),
        tt.Time.from_now(weeks=20),
        tt.Asset.Tbd(1000000),
        "subject-1",
        "permlink",
    )


def test_update_proposal(wallet: tt.Wallet) -> None:
    wallet.create_account("alice", hbds=tt.Asset.Tbd(1000000), vests=tt.Asset.Test(1000000))
    wallet.api.post_comment("initminer", "permlink", "", "parent-permlink", "title", "body", "{}")
    wallet.api.create_proposal(
        "initminer",
        "initminer",
        tt.Time.from_now(weeks=16),
        tt.Time.from_now(weeks=20),
        tt.Asset.Tbd(1000000),
        "subject-1",
        "permlink",
    )

    wallet.api.update_proposal(
        0,
        "initminer",
        tt.Asset.Tbd(1000000),
        "subject-1",
        "permlink",
        tt.Time.from_now(weeks=19),
    )


def test_remove_proposal(wallet: tt.Wallet) -> None:
    wallet.create_account("alice", hbds=tt.Asset.Tbd(1000000), vests=tt.Asset.Test(1000000))
    wallet.api.post_comment("initminer", "permlink", "", "parent-permlink", "title", "body", "{}")
    wallet.api.create_proposal(
        "initminer",
        "initminer",
        tt.Time.from_now(weeks=16),
        tt.Time.from_now(weeks=20),
        tt.Asset.Tbd(1000000),
        "subject-1",
        "permlink",
    )

    wallet.api.remove_proposal(
        "initminer",
        [0],
    )


def test_update_proposal_votes(wallet: tt.Wallet) -> None:
    wallet.create_account("alice", hbds=tt.Asset.Tbd(1000000), vests=tt.Asset.Test(1000000))
    wallet.api.post_comment("initminer", "permlink", "", "parent-permlink", "title", "body", "{}")
    wallet.api.create_proposal(
        "initminer",
        "initminer",
        tt.Time.from_now(weeks=16),
        tt.Time.from_now(weeks=20),
        tt.Asset.Tbd(1000000),
        "subject-1",
        "permlink",
    )

    wallet.api.update_proposal_votes("initminer", [0], True)


def test_change_recovery_account(wallet: tt.Wallet) -> None:
    wallet.api.change_recovery_account("initminer", "initminer")


def test_convert_hbd(wallet: tt.Wallet) -> None:
    wallet.api.convert_hbd("initminer", tt.Asset.Tbd(1.25))


def test_convert_hive_with_collateral(wallet: tt.Wallet) -> None:
    wallet.api.convert_hive_with_collateral("initminer", tt.Asset.Test(10))


def test_decline_voting_rights(wallet: tt.Wallet) -> None:
    wallet.api.decline_voting_rights("initminer", True)


def test_decrypt_memo(wallet: tt.Wallet) -> None:
    wallet.api.decline_voting_rights("initminer", True)


def test_delegate_vesting_shares(wallet: tt.Wallet) -> None:
    wallet.api.create_account("initminer", "tipu", "{}")
    wallet.api.delegate_vesting_shares("initminer", "tipu", tt.Asset.Vest(0.005))


def test_delegate_vesting_shares_and_transfer(wallet: tt.Wallet) -> None:
    wallet.api.create_account("initminer", "alice", "{}")
    wallet.api.delegate_vesting_shares_and_transfer(
        "initminer", "alice", tt.Asset.Vest(0.0001), tt.Asset.Test(6), "transfer_memo"
    )


def test_encrow_release(wallet: tt.Wallet) -> None:
    wallet.create_account("alice", vests=tt.Asset.Test(50))
    wallet.create_account("bob", vests=tt.Asset.Test(50))

    wallet.api.escrow_transfer(
        "initminer",
        "alice",
        "bob",
        99,
        tt.Asset.Tbd(1),
        tt.Asset.Test(2),
        tt.Asset.Tbd(1),
        tt.Time.from_now(weeks=16),
        tt.Time.from_now(weeks=20),
        "{}",
    )

    wallet.api.escrow_approve("initminer", "alice", "bob", "bob", 99, True)

    wallet.api.escrow_approve("initminer", "alice", "bob", "alice", 99, True)

    wallet.api.escrow_dispute("initminer", "alice", "bob", "initminer", 99)

    wallet.api.escrow_release("initminer", "alice", "bob", "bob", "alice", 99, tt.Asset.Tbd(1), tt.Asset.Test(2))


def test_escrow_dispute(wallet: tt.Wallet) -> None:
    wallet.create_account("alice", vests=tt.Asset.Test(50))
    wallet.create_account("bob", vests=tt.Asset.Test(50))

    wallet.api.escrow_transfer(
        "initminer",
        "alice",
        "bob",
        99,
        tt.Asset.Tbd(1),
        tt.Asset.Test(2),
        tt.Asset.Tbd(1),
        tt.Time.from_now(weeks=16),
        tt.Time.from_now(weeks=20),
        "{}",
    )

    wallet.api.escrow_approve("initminer", "alice", "bob", "bob", 99, True)

    wallet.api.escrow_approve("initminer", "alice", "bob", "alice", 99, True)

    wallet.api.escrow_dispute("initminer", "alice", "bob", "initminer", 99)


def test_escrow_approve(wallet: tt.Wallet) -> None:
    wallet.create_account("alice", vests=tt.Asset.Test(50))
    wallet.create_account("bob", vests=tt.Asset.Test(50))

    wallet.api.escrow_transfer(
        "initminer",
        "alice",
        "bob",
        99,
        tt.Asset.Tbd(1),
        tt.Asset.Test(2),
        tt.Asset.Tbd(1),
        tt.Time.from_now(weeks=16),
        tt.Time.from_now(weeks=20),
        "{}",
    )

    wallet.api.escrow_approve("initminer", "alice", "bob", "bob", 99, True)


def test_escrow_transfer(wallet: tt.Wallet) -> None:
    wallet.create_account("alice", vests=tt.Asset.Test(50))
    wallet.create_account("bob", vests=tt.Asset.Test(50))

    wallet.api.escrow_transfer(
        "initminer",
        "alice",
        "bob",
        99,
        tt.Asset.Tbd(1),
        tt.Asset.Test(2),
        tt.Asset.Tbd(1),
        tt.Time.from_now(weeks=16),
        tt.Time.from_now(weeks=20),
        "{}",
    )


def test_is_locked(wallet: tt.Wallet) -> None:
    wallet.api.is_locked()


def test_publish_feed(wallet: tt.Wallet) -> None:
    wallet.api.publish_feed("initminer", {"base": tt.Asset.Tbd(1), "quote": tt.Asset.Test(2)})


def test_recurrent_transfer(wallet: tt.Wallet) -> None:
    wallet.create_account("alice", hives=tt.Asset.Test(100), vests=tt.Asset.Test(100))
    wallet.api.create_account("initminer", "bob", "{}")
    wallet.api.recurrent_transfer("alice", "bob", tt.Asset.Test(5), "memo", 720, 12)


def test_recurrent_transfer_with_id(wallet: tt.Wallet) -> None:
    wallet.create_account("alice", hives=tt.Asset.Test(100), vests=tt.Asset.Test(100))
    wallet.api.create_account("initminer", "bob", "{}")
    wallet.api.recurrent_transfer_with_id("alice", "bob", tt.Asset.Test(5), "memo", 720, 12, 1)


def test_set_voting_proxy(wallet: tt.Wallet) -> None:
    wallet.create_account("alice", vests=tt.Asset.Test(1))
    wallet.create_account("bob")

    wallet.api.set_voting_proxy("alice", "bob")


def test_set_withdraw_vesting_route(wallet: tt.Wallet) -> None:
    wallet.create_account("blocktrades", vests=tt.Asset.Test(100))

    wallet.api.create_account("blocktrades", "alice", "{}")
    wallet.api.set_withdraw_vesting_route("blocktrades", "alice", 15, True)


def test_transfer_to_savings(wallet: tt.Wallet) -> None:
    wallet.create_account("alice", hives=tt.Asset.Test(1000), vests=tt.Asset.Test(1000000), hbds=tt.Asset.Tbd(1000))
    wallet.create_account("bob", hives=tt.Asset.Test(1000), vests=tt.Asset.Test(1000000), hbds=tt.Asset.Tbd(1000))

    wallet.api.transfer_to_savings("initminer", "alice", tt.Asset.Test(10), "memo")


def test_transfer_from_savings(wallet: tt.Wallet) -> None:
    wallet.create_account("alice", hives=tt.Asset.Test(1000), vests=tt.Asset.Test(1000000), hbds=tt.Asset.Tbd(1000))
    wallet.create_account("bob", hives=tt.Asset.Test(1000), vests=tt.Asset.Test(1000000), hbds=tt.Asset.Tbd(1000))

    wallet.api.transfer_to_savings("initminer", "alice", tt.Asset.Test(10), "memo")
    wallet.api.transfer_from_savings("alice", 1, "bob", tt.Asset.Test(1), "memo")


def test_update_account(wallet: tt.Wallet) -> None:
    wallet.create_account("alice", hives=tt.Asset.Test(1000), vests=tt.Asset.Test(1000000), hbds=tt.Asset.Tbd(1000))
    wallet.api.transfer_to_vesting("initminer", "alice", tt.Asset.Test(500))
    key = "STM8grZpsMPnH7sxbMVZHWEu1D26F3GwLW1fYnZEuwzT4Rtd57AER"
    wallet.api.update_account("alice", "{}", key, key, key, key)


def test_update_account_auth_account(wallet: tt.Wallet) -> None:
    wallet.create_account("alice")

    wallet.api.transfer_to_vesting("initminer", "alice", tt.Asset.Test(500))
    wallet.api.update_account_auth_account("alice", "posting", "initminer", 2)


def test_update_account_auth_key(wallet: tt.Wallet) -> None:
    wallet.create_account("alice", hives=tt.Asset.Test(100), vests=tt.Asset.Test(100))
    wallet.api.update_account_auth_key("alice", "owner", tt.Account("some-key").public_key, 1)


def test_update_account_auth_threshold(wallet: tt.Wallet) -> None:
    wallet.create_account("alice", hives=tt.Asset.Test(100), vests=tt.Asset.Test(100))
    wallet.api.update_account_auth_threshold("alice", "posting", 4)


def test_update_account_memo_key(wallet: tt.Wallet) -> None:
    wallet.create_account("alice", hives=tt.Asset.Test(100), vests=tt.Asset.Test(100))
    wallet.api.update_account_memo_key("alice", tt.Account("bob").public_key)


def test_update_account_meta(wallet: tt.Wallet) -> None:
    wallet.create_account("alice", hives=tt.Asset.Test(100), vests=tt.Asset.Test(100))
    wallet.api.update_account_meta("alice", '{ "test" : 4 }')


def test_update_witness(wallet: tt.Wallet) -> None:
    wallet.create_account("alice", hives=tt.Asset.Test(100), vests=tt.Asset.Test(100))
    wallet.api.update_witness(
        "alice",
        "http://url.html",
        tt.Account("alice").public_key,
        {"account_creation_fee": tt.Asset.Test(28), "maximum_block_size": 131072, "hbd_interest_rate": 1000},
    )


def test_vote_for_witness(wallet: tt.Wallet) -> None:
    wallet.create_account("alice", vests=tt.Asset.Test(100))
    wallet.create_account("bob", vests=tt.Asset.Test(100))

    wallet.api.update_witness(
        "alice",
        "http://url.html",
        tt.Account("alice").public_key,
        {"account_creation_fee": tt.Asset.Test(28), "maximum_block_size": 131072, "hbd_interest_rate": 1000},
    )
    wallet.api.vote_for_witness("bob", "alice", True)


def test_withdraw_vesting(wallet: tt.Wallet) -> None:
    wallet.create_account("alice", vests=tt.Asset.Test(500000))
    wallet.api.withdraw_vesting("alice", tt.Asset.Vest(10))


def test_claim_account_creation(node: tt.AnyNode, wallet: tt.Wallet) -> None:
    node.wait_number_of_blocks(30)

    wallet.api.claim_account_creation("initminer", tt.Asset.Test(0))


def test_normalize_brain_key(wallet: tt.Wallet) -> None:
    assert wallet.api.normalize_brain_key("     mango Apple CHERRY ") == "MANGO APPLE CHERRY"


def test_list_my_accounts(wallet: tt.Wallet) -> None:
    wallet.api.create_account("initminer", "alice", "{}")
    wallet.api.create_account("initminer", "bob", "{}")

    wallet.api.list_my_accounts()


def test_serialize_transaction(wallet: tt.Wallet) -> None:
    trx = wallet.api.create_account("initminer", "alice", "{}", broadcast=False)
    wallet.api.serialize_transaction(tx=trx)
