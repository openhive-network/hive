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
    'wallet_with_type_of_serialization', TYPES_OF_SERIALIZATION, indirect=True
)
def test_cancel_order(replayed_node, wallet_with_type_of_serialization, request, verify_functor):
    wallet, type_of_serialization, pattern_name = wallet_with_type_of_serialization

    transaction = wallet.api.cancel_order('alice', 1, broadcast=False)
    replayed_node.api.wallet_bridge.broadcast_transaction(transaction)

    compare_with_pattern(verify_functor, wallet, type_of_serialization, request, pattern_name)


@pytest.mark.parametrize(
    'way_to_verify_pattern', WAYS_OF_PATTERN_VERIFICATION
)
@pytest.mark.testnet
@pytest.mark.parametrize(
    'wallet_with_type_of_serialization', TYPES_OF_SERIALIZATION, indirect=True
)
def test_cancel_transfer_from_savings(replayed_node, wallet_with_type_of_serialization, request, verify_functor):
    wallet, type_of_serialization, pattern_name = wallet_with_type_of_serialization

    transaction = wallet.api.cancel_transfer_from_savings('alice', 1, broadcast=False)
    replayed_node.api.wallet_bridge.broadcast_transaction(transaction)

    compare_with_pattern(verify_functor, wallet, type_of_serialization, request, pattern_name)


@pytest.mark.parametrize(
    'way_to_verify_pattern', WAYS_OF_PATTERN_VERIFICATION
)
@pytest.mark.testnet
@pytest.mark.parametrize(
    'wallet_with_type_of_serialization', TYPES_OF_SERIALIZATION, indirect=True
)
def test_change_recovery_account(replayed_node, wallet_with_type_of_serialization, request, verify_functor):
    wallet, type_of_serialization, pattern_name = wallet_with_type_of_serialization

    transaction = wallet.api.change_recovery_account('initminer', 'alice', broadcast=False)
    replayed_node.api.wallet_bridge.broadcast_transaction(transaction)

    compare_with_pattern(verify_functor, wallet, type_of_serialization, request, pattern_name)


@pytest.mark.parametrize(
    'way_to_verify_pattern', WAYS_OF_PATTERN_VERIFICATION
)
@pytest.mark.testnet
@pytest.mark.parametrize(
    'wallet_with_type_of_serialization', TYPES_OF_SERIALIZATION, indirect=True
)
def test_claim_account_creation(replayed_node, wallet_with_type_of_serialization, request, verify_functor):
    wallet, type_of_serialization, pattern_name = wallet_with_type_of_serialization

    transaction = wallet.api.claim_account_creation('initminer', tt.Asset.Test(0), broadcast=False)
    replayed_node.api.wallet_bridge.broadcast_transaction(transaction)

    compare_with_pattern(verify_functor, wallet, type_of_serialization, request, pattern_name)


@pytest.mark.parametrize(
    'way_to_verify_pattern', WAYS_OF_PATTERN_VERIFICATION
)
@pytest.mark.testnet
@pytest.mark.parametrize(
    'wallet_with_type_of_serialization', TYPES_OF_SERIALIZATION, indirect=True
)
def test_claim_account_creation_nonblocking(replayed_node, wallet_with_type_of_serialization, request, verify_functor):
    wallet, type_of_serialization, pattern_name = wallet_with_type_of_serialization

    transaction = wallet.api.claim_account_creation_nonblocking('initminer', tt.Asset.Test(0), broadcast=False)
    replayed_node.api.wallet_bridge.broadcast_transaction(transaction)

    compare_with_pattern(verify_functor, wallet, type_of_serialization, request, pattern_name)


@pytest.mark.parametrize(
    'way_to_verify_pattern', WAYS_OF_PATTERN_VERIFICATION
)
@pytest.mark.testnet
@pytest.mark.parametrize(
    'wallet_with_type_of_serialization', TYPES_OF_SERIALIZATION, indirect=True
)
def test_create_account_with_keys(replayed_node, wallet_with_type_of_serialization, request, verify_functor):
    wallet, type_of_serialization, pattern_name = wallet_with_type_of_serialization

    key = 'TST8grZpsMPnH7sxbMVZHWEu1D26F3GwLW1fYnZEuwzT4Rtd57AER'
    transaction = wallet.api.create_account_with_keys('initminer', 'dan', '{}', key, key, key, key, broadcast=False)
    replayed_node.api.wallet_bridge.broadcast_transaction(transaction)

    compare_with_pattern(verify_functor, wallet, type_of_serialization, request, pattern_name)


@pytest.mark.parametrize(
    'way_to_verify_pattern', WAYS_OF_PATTERN_VERIFICATION
)
@pytest.mark.testnet
@pytest.mark.parametrize(
    'wallet_with_type_of_serialization', TYPES_OF_SERIALIZATION, indirect=True
)
def test_create_funded_account_with_keys(replayed_node, wallet_with_type_of_serialization, request, verify_functor):
    wallet, type_of_serialization, pattern_name = wallet_with_type_of_serialization

    key = 'TST8grZpsMPnH7sxbMVZHWEu1D26F3GwLW1fYnZEuwzT4Rtd57AER'
    transaction = wallet.api.create_funded_account_with_keys('initminer', 'dan', tt.Asset.Test(10), 'memo', '{}', key,
                                                             key, key, key, broadcast=False)
    replayed_node.api.wallet_bridge.broadcast_transaction(transaction)

    compare_with_pattern(verify_functor, wallet, type_of_serialization, request, pattern_name)


@pytest.mark.parametrize(
    'way_to_verify_pattern', WAYS_OF_PATTERN_VERIFICATION
)
@pytest.mark.testnet
@pytest.mark.parametrize(
    'wallet_with_type_of_serialization', TYPES_OF_SERIALIZATION, indirect=True
)
def test_create_proposal(replayed_node, wallet_with_type_of_serialization, request, verify_functor):
    wallet, type_of_serialization, pattern_name = wallet_with_type_of_serialization

    transaction = wallet.api.create_proposal('alice', 'alice', '2031-01-01T00:00:00', '2031-06-01T00:00:00',
                                             tt.Asset.Tbd(1000), 'subject-1', 'permlink', broadcast=False)
    replayed_node.api.wallet_bridge.broadcast_transaction(transaction)

    compare_with_pattern(verify_functor, wallet, type_of_serialization, request, pattern_name)
