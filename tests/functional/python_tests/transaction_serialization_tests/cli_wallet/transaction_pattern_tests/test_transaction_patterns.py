import pytest

import test_tools as tt
from hive_local_tools.functional.python.transaction_serialization.cli_wallet.transaction_pattern import (
    verify_generated_transaction_with_binary_pattern,
    verify_generated_transaction_with_json_pattern,
)

WAYS_OF_PATTERN_VERIFICATION = [
    verify_generated_transaction_with_binary_pattern,
    verify_generated_transaction_with_json_pattern,
]

TYPES_OF_SERIALIZATION = [
    "legacy",
    "hf26",
]

METHODS_WITH_CORRECT_ARGUMENTS = [
    ("cancel_order", ("alice", 1, False)),
    ("cancel_transfer_from_savings", ("alice", 1, False)),
    ("change_recovery_account", ("initminer", "alice", False)),
    ("claim_account_creation", ("initminer", tt.Asset.Test(0), False)),
    ("claim_account_creation_nonblocking", ("initminer", tt.Asset.Test(0), False)),
    (
        "create_proposal",
        (
            "alice",
            "alice",
            "2031-01-01T00:00:00",
            "2031-06-01T00:00:00",
            tt.Asset.Tbd(1000),
            "subject-1",
            "permlink",
            False,
        ),
    ),
    ("create_order", ("alice", 1000, tt.Asset.Test(1), tt.Asset.Tbd(1), False, 1000, False)),
    ("decline_voting_rights", ("initminer", True, False)),
    ("delegate_rc", ("alice", ["bob"], 10, False)),
    ("delegate_vesting_shares", ("alice", "bob", tt.Asset.Vest(1), False)),
    ("delegate_vesting_shares_and_transfer", ("alice", "bob", tt.Asset.Vest(1), tt.Asset.Tbd(6), "memo", False)),
    (
        "delegate_vesting_shares_and_transfer_nonblocking",
        ("alice", "bob", tt.Asset.Vest(1), tt.Asset.Tbd(6), "memo", False),
    ),
    ("delegate_vesting_shares_nonblocking", ("alice", "bob", tt.Asset.Vest(1), False)),
    ("escrow_release", ("initminer", "alice", "bob", "bob", "alice", 1, tt.Asset.Tbd(10), tt.Asset.Test(10), False)),
    (
        "escrow_transfer",
        (
            "initminer",
            "alice",
            "bob",
            10,
            tt.Asset.Tbd(10),
            tt.Asset.Test(10),
            tt.Asset.Tbd(10),
            "2030-01-01T00:00:00",
            "2030-06-01T00:00:00",
            "{}",
            False,
        ),
    ),
    ("escrow_approve", ("initminer", "alice", "bob", "bob", 2, True, False)),
    ("escrow_dispute", ("initminer", "alice", "bob", "initminer", 3, False)),
    ("recurrent_transfer", ("alice", "bob", tt.Asset.Test(5), "memo", 720, 12, False)),
    ("follow", ("alice", "bob", ["blog"], False)),
    ("post_comment", ("bob", "test-permlink", "", "someone", "test-title", "this is a body", "{}", False)),
    ("set_voting_proxy", ("initminer", "alice", False)),
    ("set_withdraw_vesting_route", ("alice", "bob", 30, True, False)),
    ("transfer", ("initminer", "alice", tt.Asset.Test(10), "memo", False)),
    ("transfer_from_savings", ("alice", 1000, "bob", tt.Asset.Test(1), "memo", False)),
    ("transfer_nonblocking", ("initminer", "alice", tt.Asset.Test(10), "memo", False)),
    ("transfer_to_savings", ("initminer", "alice", tt.Asset.Test(100), "memo", False)),
    ("transfer_to_vesting", ("initminer", "alice", tt.Asset.Test(100), False)),
    ("transfer_to_vesting_nonblocking", ("initminer", "alice", tt.Asset.Test(100), False)),
    ("update_account_auth_account", ("alice", "posting", "initminer", 2, False)),
    ("update_account_auth_threshold", ("alice", "posting", 4, False)),
    ("update_account_meta", ("alice", '{ "test" : 4 }', False)),
    ("update_proposal", (0, "alice", tt.Asset.Tbd(10), "subject-1", "permlink", "2031-05-01T00:00:00", False)),
    ("update_proposal_votes", ("alice", [0], True, False)),
    (
        "update_witness",
        (
            "alice",
            "http://url.html",
            tt.Account("alice").public_key,
            {"account_creation_fee": tt.Asset.Test(10)},
            False,
        ),
    ),
    ("vote", ("initminer", "alice", "permlink", 10, False)),
    ("vote_for_witness", ("alice", "initminer", True, False)),
    ("withdraw_vesting", ("alice", tt.Asset.Vest(10), False)),
]


@pytest.mark.testnet
@pytest.mark.parametrize("cli_wallet_method, arguments", METHODS_WITH_CORRECT_ARGUMENTS)
@pytest.mark.parametrize("verify_pattern", WAYS_OF_PATTERN_VERIFICATION)
@pytest.mark.parametrize("wallet_with_pattern_name", TYPES_OF_SERIALIZATION, indirect=True)
def test_transaction_patterns(replayed_node, wallet_with_pattern_name, verify_pattern, cli_wallet_method, arguments):
    wallet, pattern_name = wallet_with_pattern_name

    transaction = getattr(wallet.api, cli_wallet_method)(*arguments)
    replayed_node.api.wallet_bridge.broadcast_transaction(transaction)

    verify_pattern(wallet, pattern_name)


@pytest.mark.testnet
@pytest.mark.parametrize("verify_pattern", WAYS_OF_PATTERN_VERIFICATION)
@pytest.mark.parametrize("wallet_with_pattern_name", TYPES_OF_SERIALIZATION, indirect=True)
def test_create_account_with_keys(replayed_node, wallet_with_pattern_name, verify_pattern):
    wallet, pattern_name = wallet_with_pattern_name

    dan = tt.Account("dan")
    transaction = wallet.api.create_account_with_keys(
        "initminer", dan.name, "{}", dan.public_key, dan.public_key, dan.public_key, dan.public_key, broadcast=False
    )
    replayed_node.api.wallet_bridge.broadcast_transaction(transaction)

    verify_pattern(wallet, pattern_name)


@pytest.mark.testnet
@pytest.mark.parametrize("verify_pattern", WAYS_OF_PATTERN_VERIFICATION)
@pytest.mark.parametrize("wallet_with_pattern_name", TYPES_OF_SERIALIZATION, indirect=True)
def test_create_funded_account_with_keys(replayed_node, wallet_with_pattern_name, verify_pattern):
    wallet, pattern_name = wallet_with_pattern_name

    dan = tt.Account("dan")
    transaction = wallet.api.create_funded_account_with_keys(
        "initminer",
        dan.name,
        tt.Asset.Test(10),
        "memo",
        "{}",
        dan.public_key,
        dan.public_key,
        dan.public_key,
        dan.public_key,
        broadcast=False,
    )
    replayed_node.api.wallet_bridge.broadcast_transaction(transaction)

    verify_pattern(wallet, pattern_name)


@pytest.mark.testnet
@pytest.mark.parametrize("verify_pattern", WAYS_OF_PATTERN_VERIFICATION)
@pytest.mark.parametrize("wallet_with_pattern_name", TYPES_OF_SERIALIZATION, indirect=True)
def test_publish_feed(replayed_node, wallet_with_pattern_name, verify_pattern):
    wallet, pattern_name = wallet_with_pattern_name

    feed_history = wallet.api.get_feed_history()
    current_median_history = feed_history["current_median_history"]
    transaction = wallet.api.publish_feed("initminer", current_median_history, broadcast=False)
    replayed_node.api.wallet_bridge.broadcast_transaction(transaction)

    verify_pattern(wallet, pattern_name)


@pytest.mark.testnet
@pytest.mark.parametrize("verify_pattern", WAYS_OF_PATTERN_VERIFICATION)
@pytest.mark.parametrize("wallet_with_pattern_name", TYPES_OF_SERIALIZATION, indirect=True)
def test_recover_account(replayed_node, wallet_with_pattern_name, verify_pattern):
    wallet, pattern_name = wallet_with_pattern_name

    initminer_owner_key = tt.Account("initminer").public_key
    authority = {"weight_threshold": 1, "account_auths": [], "key_auths": [[initminer_owner_key, 1]]}

    thief_owner_key = tt.Account("thief").public_key

    alice_owner_key = tt.Account("alice").public_key
    recent_authority = {"weight_threshold": 1, "account_auths": [], "key_auths": [[alice_owner_key, 1]]}

    for transaction in [
        wallet.api.update_account_auth_key("alice", "owner", thief_owner_key, 3, broadcast=False),  # Account theft
        wallet.api.request_account_recovery("initminer", "alice", authority, broadcast=False),
        wallet.api.recover_account("alice", recent_authority, authority, broadcast=False),
    ]:
        replayed_node.api.wallet_bridge.broadcast_transaction(transaction)

    verify_pattern(wallet, pattern_name)


@pytest.mark.testnet
@pytest.mark.parametrize("verify_pattern", WAYS_OF_PATTERN_VERIFICATION)
@pytest.mark.parametrize("wallet_with_pattern_name", TYPES_OF_SERIALIZATION, indirect=True)
def test_update_account_auth_key(replayed_node, wallet_with_pattern_name, verify_pattern):
    wallet, pattern_name = wallet_with_pattern_name

    alice_owner_key = tt.Account("alice").public_key
    transaction = wallet.api.update_account_auth_key("alice", "owner", alice_owner_key, 3, broadcast=False)

    replayed_node.api.wallet_bridge.broadcast_transaction(transaction)

    verify_pattern(wallet, pattern_name)


@pytest.mark.testnet
@pytest.mark.parametrize("verify_pattern", WAYS_OF_PATTERN_VERIFICATION)
@pytest.mark.parametrize("wallet_with_pattern_name", TYPES_OF_SERIALIZATION, indirect=True)
def test_request_account_recovery(replayed_node, wallet_with_pattern_name, verify_pattern):
    wallet, pattern_name = wallet_with_pattern_name

    alice_owner_key = tt.Account("alice").public_key

    authority = {"weight_threshold": 1, "account_auths": [], "key_auths": [[alice_owner_key, 1]]}

    transaction = wallet.api.request_account_recovery("initminer", "alice", authority, broadcast=False)
    replayed_node.api.wallet_bridge.broadcast_transaction(transaction)

    verify_pattern(wallet, pattern_name)


@pytest.mark.testnet
@pytest.mark.parametrize("verify_pattern", WAYS_OF_PATTERN_VERIFICATION)
@pytest.mark.parametrize("wallet_with_pattern_name", TYPES_OF_SERIALIZATION, indirect=True)
def test_remove_proposal(replayed_node, wallet_with_pattern_name, verify_pattern):
    wallet, pattern_name = wallet_with_pattern_name

    account_id = wallet.api.get_account("alice")["id"]

    transaction = wallet.api.remove_proposal("initminer", [account_id], broadcast=False)
    replayed_node.api.wallet_bridge.broadcast_transaction(transaction)

    verify_pattern(wallet, pattern_name)


@pytest.mark.testnet
@pytest.mark.parametrize("verify_pattern", WAYS_OF_PATTERN_VERIFICATION)
@pytest.mark.parametrize("wallet_with_pattern_name", TYPES_OF_SERIALIZATION, indirect=True)
def test_update_account(replayed_node, wallet_with_pattern_name, verify_pattern):
    wallet, pattern_name = wallet_with_pattern_name

    key = tt.Account("alice", secret="other_than_previous").public_key
    transaction = wallet.api.update_account("alice", "{}", key, key, key, key, broadcast=False)
    replayed_node.api.wallet_bridge.broadcast_transaction(transaction)

    verify_pattern(wallet, pattern_name)


@pytest.mark.testnet
@pytest.mark.parametrize("verify_pattern", WAYS_OF_PATTERN_VERIFICATION)
@pytest.mark.parametrize("wallet_with_pattern_name", TYPES_OF_SERIALIZATION, indirect=True)
def test_update_account_memo_key(replayed_node, wallet_with_pattern_name, verify_pattern):
    wallet, pattern_name = wallet_with_pattern_name

    key = tt.Account("alice", secret="other_than_previous").public_key
    transaction = wallet.api.update_account_memo_key("alice", key, broadcast=False)
    replayed_node.api.wallet_bridge.broadcast_transaction(transaction)

    verify_pattern(wallet, pattern_name)
