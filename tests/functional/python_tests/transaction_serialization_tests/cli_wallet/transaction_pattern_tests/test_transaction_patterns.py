import pytest

import test_tools as tt

from .local_tools import verify_generated_transaction_with_json_pattern, \
    verify_generated_transaction_with_binary_pattern

WAYS_OF_PATTERN_VERIFICATION = [
    verify_generated_transaction_with_binary_pattern,
    verify_generated_transaction_with_json_pattern
]

TYPES_OF_SERIALIZATION = [
    'legacy',
    'hf26',
]


@pytest.mark.testnet
@pytest.mark.parametrize('verify_pattern', WAYS_OF_PATTERN_VERIFICATION)
@pytest.mark.parametrize('wallet_with_pattern_name', TYPES_OF_SERIALIZATION, indirect=True)
def test_cancel_order(replayed_node, wallet_with_pattern_name, verify_pattern):
    wallet, pattern_name = wallet_with_pattern_name

    transaction = wallet.api.cancel_order('alice', 1, broadcast=False)
    replayed_node.api.wallet_bridge.broadcast_transaction(transaction)

    verify_pattern(wallet, pattern_name)


@pytest.mark.testnet
@pytest.mark.parametrize('verify_pattern', WAYS_OF_PATTERN_VERIFICATION)
@pytest.mark.parametrize('wallet_with_pattern_name', TYPES_OF_SERIALIZATION, indirect=True)
def test_cancel_transfer_from_savings(replayed_node, wallet_with_pattern_name, verify_pattern):
    wallet, pattern_name = wallet_with_pattern_name

    transaction = wallet.api.cancel_transfer_from_savings('alice', 1, broadcast=False)
    replayed_node.api.wallet_bridge.broadcast_transaction(transaction)

    verify_pattern(wallet, pattern_name)


@pytest.mark.testnet
@pytest.mark.parametrize('verify_pattern', WAYS_OF_PATTERN_VERIFICATION)
@pytest.mark.parametrize('wallet_with_pattern_name', TYPES_OF_SERIALIZATION, indirect=True)
def test_change_recovery_account(replayed_node, wallet_with_pattern_name, verify_pattern):
    wallet, pattern_name = wallet_with_pattern_name

    transaction = wallet.api.change_recovery_account('initminer', 'alice', broadcast=False)
    replayed_node.api.wallet_bridge.broadcast_transaction(transaction)

    verify_pattern(wallet, pattern_name)


@pytest.mark.testnet
@pytest.mark.parametrize('verify_pattern', WAYS_OF_PATTERN_VERIFICATION)
@pytest.mark.parametrize('wallet_with_pattern_name', TYPES_OF_SERIALIZATION, indirect=True)
def test_claim_account_creation(replayed_node, wallet_with_pattern_name, verify_pattern):
    wallet, pattern_name = wallet_with_pattern_name

    transaction = wallet.api.claim_account_creation('initminer', tt.Asset.Test(0), broadcast=False)
    replayed_node.api.wallet_bridge.broadcast_transaction(transaction)

    verify_pattern(wallet, pattern_name)


@pytest.mark.testnet
@pytest.mark.parametrize('verify_pattern', WAYS_OF_PATTERN_VERIFICATION)
@pytest.mark.parametrize('wallet_with_pattern_name', TYPES_OF_SERIALIZATION, indirect=True)
def test_claim_account_creation_nonblocking(replayed_node, wallet_with_pattern_name, verify_pattern):
    wallet, pattern_name = wallet_with_pattern_name

    transaction = wallet.api.claim_account_creation_nonblocking('initminer', tt.Asset.Test(0), broadcast=False)
    replayed_node.api.wallet_bridge.broadcast_transaction(transaction)

    verify_pattern(wallet, pattern_name)


@pytest.mark.testnet
@pytest.mark.parametrize('verify_pattern', WAYS_OF_PATTERN_VERIFICATION)
@pytest.mark.parametrize('wallet_with_pattern_name', TYPES_OF_SERIALIZATION, indirect=True)
def test_create_account_with_keys(replayed_node, wallet_with_pattern_name, verify_pattern):
    wallet, pattern_name = wallet_with_pattern_name

    dan = tt.Account('dan')
    transaction = wallet.api.create_account_with_keys('initminer', dan.name, '{}', dan.public_key,
                                                      dan.public_key, dan.public_key, dan.public_key,
                                                      broadcast=False)
    replayed_node.api.wallet_bridge.broadcast_transaction(transaction)

    verify_pattern(wallet, pattern_name)


@pytest.mark.testnet
@pytest.mark.parametrize('verify_pattern', WAYS_OF_PATTERN_VERIFICATION)
@pytest.mark.parametrize('wallet_with_pattern_name', TYPES_OF_SERIALIZATION, indirect=True)
def test_create_funded_account_with_keys(replayed_node, wallet_with_pattern_name, verify_pattern):
    wallet, pattern_name = wallet_with_pattern_name

    dan = tt.Account('dan')
    transaction = wallet.api.create_funded_account_with_keys('initminer', dan.name, tt.Asset.Test(10),
                                                             'memo', '{}', dan.public_key, dan.public_key,
                                                             dan.public_key, dan.public_key, broadcast=False)
    replayed_node.api.wallet_bridge.broadcast_transaction(transaction)

    verify_pattern(wallet, pattern_name)


@pytest.mark.testnet
@pytest.mark.parametrize('verify_pattern', WAYS_OF_PATTERN_VERIFICATION)
@pytest.mark.parametrize('wallet_with_pattern_name', TYPES_OF_SERIALIZATION, indirect=True)
def test_create_proposal(replayed_node, wallet_with_pattern_name, verify_pattern):
    wallet, pattern_name = wallet_with_pattern_name

    transaction = wallet.api.create_proposal('alice', 'alice', '2031-01-01T00:00:00', '2031-06-01T00:00:00',
                                             tt.Asset.Tbd(1000), 'subject-1', 'permlink', broadcast=False)
    replayed_node.api.wallet_bridge.broadcast_transaction(transaction)

    verify_pattern(wallet, pattern_name)


@pytest.mark.testnet
@pytest.mark.parametrize('verify_pattern', WAYS_OF_PATTERN_VERIFICATION)
@pytest.mark.parametrize('wallet_with_pattern_name', TYPES_OF_SERIALIZATION, indirect=True)
def test_create_order(replayed_node, wallet_with_pattern_name, verify_pattern):
    wallet, pattern_name = wallet_with_pattern_name

    transaction = wallet.api.create_order('alice', 1000, tt.Asset.Test(1), tt.Asset.Tbd(1), False, 1000,
                                          broadcast=False)
    replayed_node.api.wallet_bridge.broadcast_transaction(transaction)

    verify_pattern(wallet, pattern_name)


@pytest.mark.testnet
@pytest.mark.parametrize('verify_pattern', WAYS_OF_PATTERN_VERIFICATION)
@pytest.mark.parametrize('wallet_with_pattern_name', TYPES_OF_SERIALIZATION, indirect=True)
def test_decline_voting_rights(replayed_node, wallet_with_pattern_name, verify_pattern):
    wallet, pattern_name = wallet_with_pattern_name

    transaction = wallet.api.decline_voting_rights('initminer', True, broadcast=False)
    replayed_node.api.wallet_bridge.broadcast_transaction(transaction)

    verify_pattern(wallet, pattern_name)


@pytest.mark.testnet
@pytest.mark.parametrize('verify_pattern', WAYS_OF_PATTERN_VERIFICATION)
@pytest.mark.parametrize('wallet_with_pattern_name', TYPES_OF_SERIALIZATION, indirect=True)
def test_delegate_rc(replayed_node, wallet_with_pattern_name, verify_pattern):
    wallet, pattern_name = wallet_with_pattern_name

    transaction = wallet.api.delegate_rc('alice', ['bob'], 10, broadcast=False)
    replayed_node.api.wallet_bridge.broadcast_transaction(transaction)

    verify_pattern(wallet, pattern_name)


@pytest.mark.testnet
@pytest.mark.parametrize('verify_pattern', WAYS_OF_PATTERN_VERIFICATION)
@pytest.mark.parametrize('wallet_with_pattern_name', TYPES_OF_SERIALIZATION, indirect=True)
def test_delegate_vesting_shares(replayed_node, wallet_with_pattern_name, verify_pattern):
    wallet, pattern_name = wallet_with_pattern_name

    transaction = wallet.api.delegate_vesting_shares('alice', 'bob', tt.Asset.Vest(1), broadcast=False)
    replayed_node.api.wallet_bridge.broadcast_transaction(transaction)

    verify_pattern(wallet, pattern_name)


@pytest.mark.testnet
@pytest.mark.parametrize('verify_pattern', WAYS_OF_PATTERN_VERIFICATION)
@pytest.mark.parametrize('wallet_with_pattern_name', TYPES_OF_SERIALIZATION, indirect=True)
def test_delegate_vesting_shares_and_transfer(replayed_node, wallet_with_pattern_name, verify_pattern):
    wallet, pattern_name = wallet_with_pattern_name

    transaction = wallet.api.delegate_vesting_shares_and_transfer('alice', 'bob', tt.Asset.Vest(1), tt.Asset.Tbd(6),
                                                                  'memo', broadcast=False)
    replayed_node.api.wallet_bridge.broadcast_transaction(transaction)

    verify_pattern(wallet, pattern_name)


@pytest.mark.testnet
@pytest.mark.parametrize('verify_pattern', WAYS_OF_PATTERN_VERIFICATION)
@pytest.mark.parametrize('wallet_with_pattern_name', TYPES_OF_SERIALIZATION, indirect=True)
def test_delegate_vesting_shares_and_transfer(replayed_node, wallet_with_pattern_name, verify_pattern):
    wallet, pattern_name = wallet_with_pattern_name

    transaction = wallet.api.delegate_vesting_shares_and_transfer_nonblocking('alice', 'bob', tt.Asset.Vest(1),
                                                                              tt.Asset.Tbd(6), 'memo', broadcast=False)
    replayed_node.api.wallet_bridge.broadcast_transaction(transaction)

    verify_pattern(wallet, pattern_name)


@pytest.mark.testnet
@pytest.mark.parametrize('verify_pattern', WAYS_OF_PATTERN_VERIFICATION)
@pytest.mark.parametrize('wallet_with_pattern_name', TYPES_OF_SERIALIZATION, indirect=True)
def test_delegate_vesting_shares_nonblocking(replayed_node, wallet_with_pattern_name, verify_pattern):
    wallet, pattern_name = wallet_with_pattern_name

    transaction = wallet.api.delegate_vesting_shares_nonblocking('alice', 'bob', tt.Asset.Vest(1), broadcast=False)
    replayed_node.api.wallet_bridge.broadcast_transaction(transaction)

    verify_pattern(wallet, pattern_name)


@pytest.mark.testnet
@pytest.mark.parametrize('verify_pattern', WAYS_OF_PATTERN_VERIFICATION)
@pytest.mark.parametrize('wallet_with_pattern_name', TYPES_OF_SERIALIZATION, indirect=True)
def test_escrow_release(replayed_node, wallet_with_pattern_name: tt.Wallet, verify_pattern):
    wallet, pattern_name = wallet_with_pattern_name

    transaction = wallet.api.escrow_release('initminer', 'alice', 'bob', 'bob', 'alice', 1, tt.Asset.Tbd(10),
                                            tt.Asset.Test(10), broadcast=False)
    replayed_node.api.wallet_bridge.broadcast_transaction(transaction)

    verify_pattern(wallet, pattern_name)


@pytest.mark.testnet
@pytest.mark.parametrize('verify_pattern', WAYS_OF_PATTERN_VERIFICATION)
@pytest.mark.parametrize('wallet_with_pattern_name', TYPES_OF_SERIALIZATION, indirect=True)
def test_escrow_transfer(replayed_node, wallet_with_pattern_name, verify_pattern):
    wallet, pattern_name = wallet_with_pattern_name

    transaction = wallet.api.escrow_transfer('initminer', 'alice', 'bob', 10, tt.Asset.Tbd(10), tt.Asset.Test(10),
                                             tt.Asset.Tbd(10), '2030-01-01T00:00:00', '2030-06-01T00:00:00', '{}',
                                             broadcast=False)
    replayed_node.api.wallet_bridge.broadcast_transaction(transaction)

    verify_pattern(wallet, pattern_name)


@pytest.mark.testnet
@pytest.mark.parametrize('verify_pattern', WAYS_OF_PATTERN_VERIFICATION)
@pytest.mark.parametrize('wallet_with_pattern_name', TYPES_OF_SERIALIZATION, indirect=True)
def test_escrow_approve(replayed_node, wallet_with_pattern_name, verify_pattern):
    wallet, pattern_name = wallet_with_pattern_name

    transaction = wallet.api.escrow_approve('initminer', 'alice', 'bob', 'bob', 2, True, broadcast=False)
    replayed_node.api.wallet_bridge.broadcast_transaction(transaction)

    verify_pattern(wallet, pattern_name)


@pytest.mark.testnet
@pytest.mark.parametrize('verify_pattern', WAYS_OF_PATTERN_VERIFICATION)
@pytest.mark.parametrize('wallet_with_pattern_name', TYPES_OF_SERIALIZATION, indirect=True)
def test_escrow_dispute(replayed_node, wallet_with_pattern_name, verify_pattern):
    wallet, pattern_name = wallet_with_pattern_name

    transaction = wallet.api.escrow_dispute('initminer', 'alice', 'bob', 'initminer', 3, broadcast=False)
    replayed_node.api.wallet_bridge.broadcast_transaction(transaction)

    verify_pattern(wallet, pattern_name)


@pytest.mark.testnet
@pytest.mark.parametrize('verify_pattern', WAYS_OF_PATTERN_VERIFICATION)
@pytest.mark.parametrize('wallet_with_pattern_name', TYPES_OF_SERIALIZATION, indirect=True)
def test_recurrent_transfer(replayed_node, wallet_with_pattern_name, verify_pattern):
    wallet, pattern_name = wallet_with_pattern_name

    transaction = wallet.api.recurrent_transfer('alice', 'bob', tt.Asset.Test(5), 'memo', 720, 12, broadcast=False)
    replayed_node.api.wallet_bridge.broadcast_transaction(transaction)

    verify_pattern(wallet, pattern_name)


@pytest.mark.testnet
@pytest.mark.parametrize('verify_pattern', WAYS_OF_PATTERN_VERIFICATION)
@pytest.mark.parametrize('wallet_with_pattern_name', TYPES_OF_SERIALIZATION, indirect=True)
def test_follow(replayed_node, wallet_with_pattern_name, verify_pattern):
    wallet, pattern_name = wallet_with_pattern_name

    transaction = wallet.api.follow('alice', 'bob', ['blog'], broadcast=False)
    replayed_node.api.wallet_bridge.broadcast_transaction(transaction)

    verify_pattern(wallet, pattern_name)


@pytest.mark.testnet
@pytest.mark.parametrize('verify_pattern', WAYS_OF_PATTERN_VERIFICATION)
@pytest.mark.parametrize('wallet_with_pattern_name', TYPES_OF_SERIALIZATION, indirect=True)
def test_post_comment(replayed_node, wallet_with_pattern_name, verify_pattern):
    wallet, pattern_name = wallet_with_pattern_name

    transaction = wallet.api.post_comment('bob', 'test-permlink', '', 'someone', 'test-title', 'this is a body', '{}',
                                          broadcast=False)
    replayed_node.api.wallet_bridge.broadcast_transaction(transaction)

    verify_pattern(wallet, pattern_name)


@pytest.mark.testnet
@pytest.mark.parametrize('verify_pattern', WAYS_OF_PATTERN_VERIFICATION)
@pytest.mark.parametrize('wallet_with_pattern_name', TYPES_OF_SERIALIZATION, indirect=True)
def test_publish_feed(replayed_node, wallet_with_pattern_name, verify_pattern):
    wallet, pattern_name = wallet_with_pattern_name

    feed_history = wallet.api.get_feed_history()
    current_median_history = feed_history['current_median_history']
    transaction = wallet.api.publish_feed('initminer', current_median_history, broadcast=False)
    replayed_node.api.wallet_bridge.broadcast_transaction(transaction)

    verify_pattern(wallet, pattern_name)


@pytest.mark.testnet
@pytest.mark.parametrize('verify_pattern', WAYS_OF_PATTERN_VERIFICATION)
@pytest.mark.parametrize('wallet_with_pattern_name', TYPES_OF_SERIALIZATION, indirect=True)
def test_change_recovery_account(replayed_node, wallet_with_pattern_name, verify_pattern):
    wallet, pattern_name = wallet_with_pattern_name

    transaction = wallet.api.change_recovery_account('alice', 'bob', broadcast=False)
    replayed_node.api.wallet_bridge.broadcast_transaction(transaction)

    verify_pattern(wallet, pattern_name)


@pytest.mark.testnet
@pytest.mark.parametrize('verify_pattern', WAYS_OF_PATTERN_VERIFICATION)
@pytest.mark.parametrize('wallet_with_pattern_name', TYPES_OF_SERIALIZATION, indirect=True)
def test_recover_account(replayed_node, wallet_with_pattern_name, verify_pattern):
    wallet, pattern_name = wallet_with_pattern_name

    initminer_owner_key = tt.Account('initminer').public_key
    authority = {"weight_threshold": 1, "account_auths": [], "key_auths": [[initminer_owner_key, 1]]}
    new_authority = {"weight_threshold": 1, "account_auths": [], "key_auths": [[initminer_owner_key, 1]]}

    alice_owner_key = tt.Account('alice').public_key
    recent_authority = {"weight_threshold": 1, "account_auths": [], "key_auths": [[alice_owner_key, 1]]}

    for transaction in [
        wallet.api.request_account_recovery('initminer', 'alice', authority, broadcast=False),
        wallet.api.update_account_auth_key('alice', 'owner', alice_owner_key, 3, broadcast=False),
        wallet.api.recover_account('alice', recent_authority, new_authority, broadcast=False),
    ]:
        replayed_node.api.wallet_bridge.broadcast_transaction(transaction)

    verify_pattern(wallet, pattern_name)


@pytest.mark.testnet
@pytest.mark.parametrize('verify_pattern', WAYS_OF_PATTERN_VERIFICATION)
@pytest.mark.parametrize('wallet_with_pattern_name', TYPES_OF_SERIALIZATION, indirect=True)
def test_update_account_auth_key(replayed_node, wallet_with_pattern_name, verify_pattern):
    wallet, pattern_name = wallet_with_pattern_name

    alice_owner_key = tt.Account('alice').public_key
    transaction = wallet.api.update_account_auth_key('alice', 'owner', alice_owner_key, 3, broadcast=False)

    authority = {"weight_threshold": 1, "account_auths": [], "key_auths": [[alice_owner_key, 1]]}

    for transaction in [
        wallet.api.request_account_recovery('alice', 'carol', authority, broadcast=False),
        wallet.api.update_account_auth_key('carol', 'owner', carol_owner_key, 3, broadcast=False),
    ]:
        replayed_node.api.wallet_bridge.broadcast_transaction(transaction)

    verify_pattern(wallet, pattern_name)


@pytest.mark.testnet
@pytest.mark.parametrize('verify_pattern', WAYS_OF_PATTERN_VERIFICATION)
@pytest.mark.parametrize('wallet_with_pattern_name', TYPES_OF_SERIALIZATION, indirect=True)
def test_request_account_recovery(replayed_node, wallet_with_pattern_name, verify_pattern):
    wallet, pattern_name = wallet_with_pattern_name

    alice_owner_key = tt.Account('alice').public_key

    authority = {"weight_threshold": 1, "account_auths": [], "key_auths": [[alice_owner_key, 1]]}

    transaction = wallet.api.request_account_recovery('initminer', 'alice', authority, broadcast=False)
    replayed_node.api.wallet_bridge.broadcast_transaction(transaction)

    verify_pattern(wallet, pattern_name)


@pytest.mark.testnet
@pytest.mark.parametrize('verify_pattern', WAYS_OF_PATTERN_VERIFICATION)
@pytest.mark.parametrize('wallet_with_pattern_name', TYPES_OF_SERIALIZATION, indirect=True)
def test_remove_proposal(replayed_node, wallet_with_pattern_name, verify_pattern):
    wallet, pattern_name = wallet_with_pattern_name

    account_id = wallet.api.get_account('alice')['id']

    transaction = wallet.api.remove_proposal('initminer', [account_id], broadcast=False)
    replayed_node.api.wallet_bridge.broadcast_transaction(transaction)

    verify_pattern(wallet, pattern_name)


@pytest.mark.testnet
@pytest.mark.parametrize('verify_pattern', WAYS_OF_PATTERN_VERIFICATION)
@pytest.mark.parametrize('wallet_with_pattern_name', TYPES_OF_SERIALIZATION, indirect=True)
def test_set_voting_proxy(replayed_node, wallet_with_pattern_name, verify_pattern):
    wallet, pattern_name = wallet_with_pattern_name

    transaction = wallet.api.set_voting_proxy('initminer', 'alice', broadcast=False)
    replayed_node.api.wallet_bridge.broadcast_transaction(transaction)

    verify_pattern(wallet, pattern_name)


@pytest.mark.testnet
@pytest.mark.parametrize('verify_pattern', WAYS_OF_PATTERN_VERIFICATION)
@pytest.mark.parametrize('wallet_with_pattern_name', TYPES_OF_SERIALIZATION, indirect=True)
def test_set_withdraw_vesting_route(replayed_node, wallet_with_pattern_name, verify_pattern):
    wallet, pattern_name = wallet_with_pattern_name

    transaction = wallet.api.set_withdraw_vesting_route('alice', 'bob', 30, True, broadcast=False)
    replayed_node.api.wallet_bridge.broadcast_transaction(transaction)

    verify_pattern(wallet, pattern_name)


@pytest.mark.testnet
@pytest.mark.parametrize('verify_pattern', WAYS_OF_PATTERN_VERIFICATION)
@pytest.mark.parametrize('wallet_with_pattern_name', TYPES_OF_SERIALIZATION, indirect=True)
def test_transfer(replayed_node, wallet_with_pattern_name, verify_pattern):
    wallet, pattern_name = wallet_with_pattern_name

    transaction = wallet.api.transfer('initminer', 'alice', tt.Asset.Test(10), 'memo', broadcast=False)
    replayed_node.api.wallet_bridge.broadcast_transaction(transaction)

    verify_pattern(wallet, pattern_name)


@pytest.mark.testnet
@pytest.mark.parametrize('verify_pattern', WAYS_OF_PATTERN_VERIFICATION)
@pytest.mark.parametrize('wallet_with_pattern_name', TYPES_OF_SERIALIZATION, indirect=True)
def test_transfer_from_savings(replayed_node, wallet_with_pattern_name, verify_pattern):
    wallet, pattern_name = wallet_with_pattern_name

    transaction = wallet.api.transfer_from_savings('alice', 1000, 'bob', tt.Asset.Test(1), 'memo', broadcast=False)
    replayed_node.api.wallet_bridge.broadcast_transaction(transaction)

    verify_pattern(wallet, pattern_name)


@pytest.mark.testnet
@pytest.mark.parametrize('verify_pattern', WAYS_OF_PATTERN_VERIFICATION)
@pytest.mark.parametrize('wallet_with_pattern_name', TYPES_OF_SERIALIZATION, indirect=True)
def test_transfer_nonblocking(replayed_node, wallet_with_pattern_name, verify_pattern):
    wallet, pattern_name = wallet_with_pattern_name

    transaction = wallet.api.transfer_nonblocking('initminer', 'alice', tt.Asset.Test(10), 'memo', broadcast=False)
    replayed_node.api.wallet_bridge.broadcast_transaction(transaction)

    verify_pattern(wallet, pattern_name)


@pytest.mark.testnet
@pytest.mark.parametrize('verify_pattern', WAYS_OF_PATTERN_VERIFICATION)
@pytest.mark.parametrize('wallet_with_pattern_name', TYPES_OF_SERIALIZATION, indirect=True)
def test_transfer_to_savings(replayed_node, wallet_with_pattern_name, verify_pattern):
    wallet, pattern_name = wallet_with_pattern_name

    transaction = wallet.api.transfer_to_savings('initminer', 'alice', tt.Asset.Test(100), 'memo', broadcast=False)
    replayed_node.api.wallet_bridge.broadcast_transaction(transaction)

    verify_pattern(wallet, pattern_name)


@pytest.mark.testnet
@pytest.mark.parametrize('verify_pattern', WAYS_OF_PATTERN_VERIFICATION)
@pytest.mark.parametrize('wallet_with_pattern_name', TYPES_OF_SERIALIZATION, indirect=True)
def test_transfer_to_vesting(replayed_node, wallet_with_pattern_name, verify_pattern):
    wallet, pattern_name = wallet_with_pattern_name

    transaction = wallet.api.transfer_to_vesting('initminer', 'alice', tt.Asset.Test(100), broadcast=False)
    replayed_node.api.wallet_bridge.broadcast_transaction(transaction)

    verify_pattern(wallet, pattern_name)


@pytest.mark.testnet
@pytest.mark.parametrize('verify_pattern', WAYS_OF_PATTERN_VERIFICATION)
@pytest.mark.parametrize('wallet_with_pattern_name', TYPES_OF_SERIALIZATION, indirect=True)
def test_transfer_to_vesting_nonblocking(replayed_node, wallet_with_pattern_name, verify_pattern):
    wallet, pattern_name = wallet_with_pattern_name

    transaction = wallet.api.transfer_to_vesting_nonblocking('initminer', 'alice', tt.Asset.Test(100), broadcast=False)
    replayed_node.api.wallet_bridge.broadcast_transaction(transaction)

    verify_pattern(wallet, pattern_name)


@pytest.mark.testnet
@pytest.mark.parametrize('verify_pattern', WAYS_OF_PATTERN_VERIFICATION)
@pytest.mark.parametrize('wallet_with_pattern_name', TYPES_OF_SERIALIZATION, indirect=True)
def test_update_account(replayed_node, wallet_with_pattern_name, verify_pattern):
    wallet, pattern_name = wallet_with_pattern_name

    key = tt.Account('alice', secret='other_than_previous').public_key
    transaction = wallet.api.update_account('alice', '{}', key, key, key, key, broadcast=False)
    replayed_node.api.wallet_bridge.broadcast_transaction(transaction)

    verify_pattern(wallet, pattern_name)


@pytest.mark.testnet
@pytest.mark.parametrize('verify_pattern', WAYS_OF_PATTERN_VERIFICATION)
@pytest.mark.parametrize('wallet_with_pattern_name', TYPES_OF_SERIALIZATION, indirect=True)
def test_update_account(replayed_node, wallet_with_pattern_name, verify_pattern):
    wallet, pattern_name = wallet_with_pattern_name

    transaction = wallet.api.update_account_auth_account('alice', 'posting', 'initminer', 2, broadcast=False)
    replayed_node.api.wallet_bridge.broadcast_transaction(transaction)

    verify_pattern(wallet, pattern_name)


@pytest.mark.testnet
@pytest.mark.parametrize('verify_pattern', WAYS_OF_PATTERN_VERIFICATION)
@pytest.mark.parametrize('wallet_with_pattern_name', TYPES_OF_SERIALIZATION, indirect=True)
def test_update_account_auth_threshold(replayed_node, wallet_with_pattern_name, verify_pattern):
    wallet, pattern_name = wallet_with_pattern_name

    transaction = wallet.api.update_account_auth_threshold('alice', 'posting', 4, broadcast=False)
    replayed_node.api.wallet_bridge.broadcast_transaction(transaction)

    verify_pattern(wallet, pattern_name)


@pytest.mark.testnet
@pytest.mark.parametrize('verify_pattern', WAYS_OF_PATTERN_VERIFICATION)
@pytest.mark.parametrize('wallet_with_pattern_name', TYPES_OF_SERIALIZATION, indirect=True)
def test_update_account_memo_key(replayed_node, wallet_with_pattern_name, verify_pattern):
    wallet, pattern_name = wallet_with_pattern_name

    key = tt.Account('alice', secret='other_than_previous').public_key
    transaction = wallet.api.update_account_memo_key('alice', key, broadcast=False)
    replayed_node.api.wallet_bridge.broadcast_transaction(transaction)

    verify_pattern(wallet, pattern_name)


@pytest.mark.testnet
@pytest.mark.parametrize('verify_pattern', WAYS_OF_PATTERN_VERIFICATION)
@pytest.mark.parametrize('wallet_with_pattern_name', TYPES_OF_SERIALIZATION, indirect=True)
def test_update_account_meta(replayed_node, wallet_with_pattern_name, verify_pattern):
    wallet, pattern_name = wallet_with_pattern_name

    transaction = wallet.api.update_account_meta('alice', '{ "test" : 4 }', broadcast=False)
    replayed_node.api.wallet_bridge.broadcast_transaction(transaction)

    verify_pattern(wallet, pattern_name)


@pytest.mark.testnet
@pytest.mark.parametrize('verify_pattern', WAYS_OF_PATTERN_VERIFICATION)
@pytest.mark.parametrize('wallet_with_pattern_name', TYPES_OF_SERIALIZATION, indirect=True)
def test_update_proposal(replayed_node, wallet_with_pattern_name, verify_pattern):
    wallet, pattern_name = wallet_with_pattern_name

    transaction = wallet.api.update_proposal(0, 'alice', tt.Asset.Tbd(10), 'subject-1', 'permlink',
                                             '2031-05-01T00:00:00', broadcast=False)
    replayed_node.api.wallet_bridge.broadcast_transaction(transaction)

    verify_pattern(wallet, pattern_name)


@pytest.mark.testnet
@pytest.mark.parametrize('verify_pattern', WAYS_OF_PATTERN_VERIFICATION)
@pytest.mark.parametrize('wallet_with_pattern_name', TYPES_OF_SERIALIZATION, indirect=True)
def test_update_proposal_votes(replayed_node, wallet_with_pattern_name, verify_pattern):
    wallet, pattern_name = wallet_with_pattern_name

    transaction = wallet.api.update_proposal_votes('alice', [0], True, broadcast=False)
    replayed_node.api.wallet_bridge.broadcast_transaction(transaction)

    verify_pattern(wallet, pattern_name)


@pytest.mark.testnet
@pytest.mark.parametrize('verify_pattern', WAYS_OF_PATTERN_VERIFICATION)
@pytest.mark.parametrize('wallet_with_pattern_name', TYPES_OF_SERIALIZATION, indirect=True)
def test_update_witness(replayed_node, wallet_with_pattern_name, verify_pattern):
    wallet, pattern_name = wallet_with_pattern_name

    transaction = wallet.api.update_witness('alice', 'http://url.html', tt.Account('alice').public_key,
                                            {'account_creation_fee': tt.Asset.Test(10)}, broadcast=False)
    replayed_node.api.wallet_bridge.broadcast_transaction(transaction)

    verify_pattern(wallet, pattern_name)


@pytest.mark.testnet
@pytest.mark.parametrize('verify_pattern', WAYS_OF_PATTERN_VERIFICATION)
@pytest.mark.parametrize('wallet_with_pattern_name', TYPES_OF_SERIALIZATION, indirect=True)
def test_vote(replayed_node, wallet_with_pattern_name, verify_pattern):
    wallet, pattern_name = wallet_with_pattern_name

    transaction = wallet.api.vote('initminer', 'alice', 'permlink', 10, broadcast=False)
    replayed_node.api.wallet_bridge.broadcast_transaction(transaction)

    verify_pattern(wallet, pattern_name)


@pytest.mark.testnet
@pytest.mark.parametrize('verify_pattern', WAYS_OF_PATTERN_VERIFICATION)
@pytest.mark.parametrize('wallet_with_pattern_name', TYPES_OF_SERIALIZATION, indirect=True)
def test_vote_for_witness(replayed_node, wallet_with_pattern_name, verify_pattern):
    wallet, pattern_name = wallet_with_pattern_name

    transaction = wallet.api.vote_for_witness('alice', 'initminer', True, broadcast=False)
    replayed_node.api.wallet_bridge.broadcast_transaction(transaction)

    verify_pattern(wallet, pattern_name)


@pytest.mark.testnet
@pytest.mark.parametrize('verify_pattern', WAYS_OF_PATTERN_VERIFICATION)
@pytest.mark.parametrize('wallet_with_pattern_name', TYPES_OF_SERIALIZATION, indirect=True)
def test_vote_for_witness(replayed_node, wallet_with_pattern_name, verify_pattern):
    wallet, pattern_name = wallet_with_pattern_name

    transaction = wallet.api.withdraw_vesting('alice', tt.Asset.Vest(10), broadcast=False)
    replayed_node.api.wallet_bridge.broadcast_transaction(transaction)

    verify_pattern(wallet, pattern_name)
