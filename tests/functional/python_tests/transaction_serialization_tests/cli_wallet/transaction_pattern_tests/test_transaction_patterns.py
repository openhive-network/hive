import pytest

import test_tools as tt

from .local_tools import compare_with_pattern, verify_correctness_of_generated_transaction_json, \
    verify_correctness_of_generated_transaction_bin

STORE_TRANSACTION = False

WAYS_OF_PATTERN_VERIFICATION = [
    verify_correctness_of_generated_transaction_bin,
    verify_correctness_of_generated_transaction_json,
]

TYPES_OF_SERIALIZATION = [
    'legacy',
    'hf26',
]


@pytest.mark.parametrize(
    'way_to_verify_pattern', WAYS_OF_PATTERN_VERIFICATION
)
@pytest.mark.testnet
@pytest.mark.parametrize(
    'wallet_with_pattern_name', TYPES_OF_SERIALIZATION, indirect=True
)
def test_cancel_order(replayed_node, wallet_with_pattern_name, way_to_verify_pattern):
    wallet, pattern_name = wallet_with_pattern_name

    transaction = wallet.api.cancel_order('alice', 1, broadcast=False)
    replayed_node.api.wallet_bridge.broadcast_transaction(transaction)

    way_to_verify_pattern(wallet, pattern_name)


@pytest.mark.parametrize(
    'way_to_verify_pattern', WAYS_OF_PATTERN_VERIFICATION
)
@pytest.mark.testnet
@pytest.mark.parametrize(
    'wallet_with_pattern_name', TYPES_OF_SERIALIZATION, indirect=True
)
def test_cancel_transfer_from_savings(replayed_node, wallet_with_pattern_name, way_to_verify_pattern):
    wallet, pattern_name = wallet_with_pattern_name

    transaction = wallet.api.cancel_transfer_from_savings('alice', 1, broadcast=False)
    replayed_node.api.wallet_bridge.broadcast_transaction(transaction)

    way_to_verify_pattern(wallet, pattern_name)


@pytest.mark.parametrize(
    'way_to_verify_pattern', WAYS_OF_PATTERN_VERIFICATION
)
@pytest.mark.testnet
@pytest.mark.parametrize(
    'wallet_with_pattern_name', TYPES_OF_SERIALIZATION, indirect=True
)
def test_change_recovery_account(replayed_node, wallet_with_pattern_name, way_to_verify_pattern):
    wallet, pattern_name = wallet_with_pattern_name

    transaction = wallet.api.change_recovery_account('initminer', 'alice', broadcast=False)
    replayed_node.api.wallet_bridge.broadcast_transaction(transaction)

    way_to_verify_pattern(wallet, pattern_name)


@pytest.mark.parametrize(
    'way_to_verify_pattern', WAYS_OF_PATTERN_VERIFICATION
)
@pytest.mark.testnet
@pytest.mark.parametrize(
    'wallet_with_pattern_name', TYPES_OF_SERIALIZATION, indirect=True
)
def test_claim_account_creation(replayed_node, wallet_with_pattern_name, way_to_verify_pattern):
    wallet, pattern_name = wallet_with_pattern_name

    transaction = wallet.api.claim_account_creation('initminer', tt.Asset.Test(0), broadcast=False)
    replayed_node.api.wallet_bridge.broadcast_transaction(transaction)

    way_to_verify_pattern(wallet, pattern_name)


@pytest.mark.parametrize(
    'way_to_verify_pattern', WAYS_OF_PATTERN_VERIFICATION
)
@pytest.mark.testnet
@pytest.mark.parametrize(
    'wallet_with_pattern_name', TYPES_OF_SERIALIZATION, indirect=True
)
def test_claim_account_creation_nonblocking(replayed_node, wallet_with_pattern_name, way_to_verify_pattern):
    wallet, pattern_name = wallet_with_pattern_name

    transaction = wallet.api.claim_account_creation_nonblocking('initminer', tt.Asset.Test(0), broadcast=False)
    replayed_node.api.wallet_bridge.broadcast_transaction(transaction)

    way_to_verify_pattern(wallet, pattern_name)


@pytest.mark.parametrize(
    'way_to_verify_pattern', WAYS_OF_PATTERN_VERIFICATION
)
@pytest.mark.testnet
@pytest.mark.parametrize(
    'wallet_with_pattern_name', TYPES_OF_SERIALIZATION, indirect=True
)
def test_create_account_with_keys(replayed_node, wallet_with_pattern_name, way_to_verify_pattern):
    wallet, pattern_name = wallet_with_pattern_name

    dan = tt.Account('dan')
    transaction = wallet.api.create_account_with_keys('initminer', dan.name, '{}', dan.public_key,
                                                      dan.public_key, dan.public_key, dan.public_key,
                                                      broadcast=False)
    replayed_node.api.wallet_bridge.broadcast_transaction(transaction)

    way_to_verify_pattern(wallet, pattern_name)


@pytest.mark.parametrize(
    'way_to_verify_pattern', WAYS_OF_PATTERN_VERIFICATION
)
@pytest.mark.testnet
@pytest.mark.parametrize(
    'wallet_with_pattern_name', TYPES_OF_SERIALIZATION, indirect=True
)
def test_create_funded_account_with_keys(replayed_node, wallet_with_pattern_name, way_to_verify_pattern):
    wallet, pattern_name = wallet_with_pattern_name

    dan = tt.Account('dan')
    transaction = wallet.api.create_funded_account_with_keys('initminer', dan.name, tt.Asset.Test(10),
                                                             'memo', '{}', dan.public_key, dan.public_key,
                                                             dan.public_key, dan.public_key, broadcast=False)
    replayed_node.api.wallet_bridge.broadcast_transaction(transaction)

    way_to_verify_pattern(wallet, pattern_name)


@pytest.mark.parametrize(
    'way_to_verify_pattern', WAYS_OF_PATTERN_VERIFICATION
)
@pytest.mark.testnet
@pytest.mark.parametrize(
    'wallet_with_pattern_name', TYPES_OF_SERIALIZATION, indirect=True
)
def test_create_proposal(replayed_node, wallet_with_pattern_name, way_to_verify_pattern):
    wallet, pattern_name = wallet_with_pattern_name

    transaction = wallet.api.create_proposal('alice', 'alice', '2031-01-01T00:00:00', '2031-06-01T00:00:00',
                                             tt.Asset.Tbd(1000), 'subject-1', 'permlink', broadcast=False)
    replayed_node.api.wallet_bridge.broadcast_transaction(transaction)

    way_to_verify_pattern(wallet, pattern_name)
