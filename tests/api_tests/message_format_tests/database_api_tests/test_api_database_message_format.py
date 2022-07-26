import json
import pytest

import test_tools as tt

from .local_tools import create_account_and_fund_it, create_and_cancel_vesting_delegation, create_proposal,\
    date_from_now, generate_sig_digest, prepare_escrow, request_account_recovery, transfer_and_withdraw_from_savings


def test_find_account_recovery_requests(node, wallet):
    wallet.api.create_account('initminer', 'alice', '{}')
    request_account_recovery(wallet, 'alice')
    requests = node.api.database.find_account_recovery_requests(accounts=['alice'])['requests']
    assert len(requests) != 0


def test_find_accounts(node, wallet):
    wallet.api.create_account('initminer', 'alice', '{}')
    accounts = node.api.database.find_accounts(accounts=['alice'])['accounts']
    assert len(accounts) != 0


def test_find_change_recovery_account_requests(node, wallet):
    create_account_and_fund_it(wallet, 'alice', tests=tt.Asset.Test(100), vests=tt.Asset.Test(100))
    wallet.api.change_recovery_account('alice', 'steem.dao')
    requests = node.api.database.find_change_recovery_account_requests(accounts=['alice'])['requests']
    assert len(requests) != 0


def test_find_collateralized_conversion_requests(node, wallet):
    create_account_and_fund_it(wallet, 'alice', tests=tt.Asset.Test(100), vests=tt.Asset.Test(100))
    # convert_hive_with_collateral changes hives for hbd, this process takes three and half days
    wallet.api.convert_hive_with_collateral('alice', tt.Asset.Test(4))
    requests = node.api.database.find_collateralized_conversion_requests(account='alice')['requests']
    assert len(requests) != 0


def test_find_comments(node, wallet):
    create_account_and_fund_it(wallet, 'alice', tests=tt.Asset.Test(100), vests=tt.Asset.Test(100))
    wallet.api.post_comment('alice', 'test-permlink', '', 'someone', 'test-title', 'this is a body', '{}')
    comments = node.api.database.find_comments(comments=[['alice', 'test-permlink']])['comments']
    assert len(comments) != 0


def test_find_decline_voting_rights_requests(node, wallet):
    create_account_and_fund_it(wallet, 'alice', tests=tt.Asset.Test(100), vests=tt.Asset.Test(100))
    wallet.api.decline_voting_rights('alice', True)
    requests = node.api.database.find_decline_voting_rights_requests(accounts=['alice'])
    assert len(requests) != 0


def test_find_escrows(node, wallet):
    prepare_escrow(wallet, sender='alice')
    # "from" is a Python keyword and needs workaround
    escrows = node.api.database.find_escrows(**{'from': 'alice'})['escrows']
    assert len(escrows) != 0


def test_find_hbd_conversion_requests(node, wallet):
    create_account_and_fund_it(wallet, 'alice', tests=tt.Asset.Test(100), vests=tt.Asset.Test(100),
                               tbds=tt.Asset.Tbd(100))
    wallet.api.convert_hbd('alice', tt.Asset.Tbd(1.25))
    requests = node.api.database.find_hbd_conversion_requests(account='alice')['requests']
    assert len(requests) != 0


def test_find_limit_orders(node, wallet):
    create_account_and_fund_it(wallet, 'alice', tests=tt.Asset.Test(100), vests=tt.Asset.Test(100))
    wallet.api.create_order('alice', 431, tt.Asset.Test(50), tt.Asset.Tbd(5), False, 3600)
    orders = node.api.database.find_limit_orders(account='alice')['orders']
    assert len(orders) != 0


def test_find_owner_histories(node, wallet):
    create_account_and_fund_it(wallet, 'alice', tests=tt.Asset.Test(100), vests=tt.Asset.Test(100))
    # update_account_auth_key with owner parameter is called to change owner authority history
    wallet.api.update_account_auth_key('alice', 'owner', tt.Account('some key').public_key, 1)
    owner_auths = node.api.database.find_owner_histories(owner='alice')['owner_auths']
    assert len(owner_auths) != 0


def test_find_proposals(node, wallet):
    create_account_and_fund_it(wallet, 'alice', tests=tt.Asset.Test(100), vests=tt.Asset.Test(100),
                               tbds=tt.Asset.Tbd(300))
    create_proposal(wallet, 'alice')
    proposals = node.api.database.find_proposals(proposal_ids=[0])['proposals']
    assert len(proposals) != 0


def test_find_recurrent_transfers(node, wallet):
    create_account_and_fund_it(wallet, 'alice', tests=tt.Asset.Test(100), vests=tt.Asset.Test(100))
    wallet.api.create_account('initminer', 'bob', '{}')
    # create transfer from alice to bob for amount 5 test hives every 720 hours, repeat 12 times
    wallet.api.recurrent_transfer('alice', 'bob', tt.Asset.Test(5), 'memo', 720, 12)
    # "from" is a Python keyword and needs workaround
    recurrent_transfers = node.api.database.find_recurrent_transfers(**{'from': 'alice'})['recurrent_transfers']
    assert len(recurrent_transfers) != 0


def test_find_savings_withdrawals(node, wallet):
    create_account_and_fund_it(wallet, 'alice', tests=tt.Asset.Test(100), vests=tt.Asset.Test(100))
    transfer_and_withdraw_from_savings(wallet, 'alice')
    withdrawals = node.api.database.find_savings_withdrawals(account='alice')['withdrawals']
    assert len(withdrawals) != 0


def test_find_vesting_delegation_expirations(node, wallet):
    create_account_and_fund_it(wallet, 'alice', tests=tt.Asset.Test(100), vests=tt.Asset.Test(100))
    wallet.api.create_account('alice', 'bob', '{}')
    create_and_cancel_vesting_delegation(wallet, 'alice', 'bob')
    delegations = node.api.database.find_vesting_delegation_expirations(account='alice')['delegations']
    assert len(delegations) != 0


def test_find_vesting_delegations(node, wallet):
    create_account_and_fund_it(wallet, 'alice', tests=tt.Asset.Test(100), vests=tt.Asset.Test(100))
    wallet.api.create_account('alice', 'bob', '{}')
    wallet.api.delegate_vesting_shares('alice', 'bob', tt.Asset.Vest(5))
    delegations = node.api.database.find_vesting_delegations(account='alice')
    assert len(delegations) != 0


def test_find_withdraw_vesting_routes(node, wallet):
    create_account_and_fund_it(wallet, 'alice', tests=tt.Asset.Test(100), vests=tt.Asset.Test(100))
    wallet.api.create_account('alice', 'bob', '{}')
    wallet.api.set_withdraw_vesting_route('alice', 'bob', 15, True)
    routes = node.api.database.find_withdraw_vesting_routes(account='bob', order='by_destination')['routes']
    assert len(routes) != 0


def test_find_witnesses(node, wallet):
    witnesses = node.api.database.find_witnesses(owners=['initminer'])['witnesses']
    assert len(witnesses) != 0


def test_get_comment_pending_payouts(node, wallet):
    create_account_and_fund_it(wallet, 'alice', tests=tt.Asset.Test(100), vests=tt.Asset.Test(100))
    wallet.api.post_comment('alice', 'test-permlink', '', 'test-parent-permlink', 'test-title', 'test-body', '{}')
    cashout_info = node.api.database.get_comment_pending_payouts(comments=[['alice', 'test-permlink']])['cashout_infos']
    assert len(cashout_info) != 0


def test_get_order_book(node, wallet):
    create_account_and_fund_it(wallet, 'alice', tests=tt.Asset.Test(100), vests=tt.Asset.Test(100))
    create_account_and_fund_it(wallet, 'bob', tests=tt.Asset.Test(100), vests=tt.Asset.Test(100),
                               tbds=tt.Asset.Tbd(100))

    wallet.api.create_order('alice', 0, tt.Asset.Test(100), tt.Asset.Tbd(100), False, 3600)  # Sell 100 HIVE for 100 HBD
    wallet.api.create_order('bob', 0, tt.Asset.Tbd(50), tt.Asset.Test(100), False, 3600)  # Buy 100 HIVE for 50 HBD

    response = node.api.database.get_order_book(limit=100)
    assert len(response['asks']) != 0
    assert len(response['bids']) != 0


def test_get_potential_signatures(node, wallet):
    transaction = wallet.api.create_account('initminer', 'alice', '{}')
    keys = node.api.database.get_potential_signatures(trx=transaction)['keys']
    assert len(keys) != 0


def test_get_required_signatures(node, wallet):
    transaction = wallet.api.create_account('initminer', 'alice', '{}')
    keys = node.api.database.get_required_signatures(trx=transaction, available_keys=[tt.Account('initminer').public_key])['keys']
    assert len(keys) != 0


def test_get_transaction_hex(node, wallet):
    transaction = wallet.api.create_account('initminer', 'alice', '{}')
    output_hex = node.api.database.get_transaction_hex(trx=transaction)['hex']
    assert len(output_hex) != 0


def test_is_known_transaction(node, wallet):
    transaction = wallet.api.create_account('initminer', 'alice', '{}')
    node.api.database.is_known_transaction(id=transaction['transaction_id'])


def test_list_account_recovery_requests(node, wallet):
    wallet.api.create_account('initminer', 'alice', '{}')
    request_account_recovery(wallet, 'alice')
    requests = node.api.database.list_account_recovery_requests(start='', limit=100, order='by_account')['requests']
    assert len(requests) != 0


def test_list_accounts(node, wallet):
    wallet.api.create_account('initminer', 'alice', '{}')
    accounts = node.api.database.list_accounts(start='', limit=100, order='by_name')['accounts']
    assert len(accounts) != 0


def test_list_change_recovery_account_requests(node, wallet):
    create_account_and_fund_it(wallet, 'alice', tests=tt.Asset.Test(100), vests=tt.Asset.Test(100))
    wallet.api.change_recovery_account('initminer', 'hive.fund')
    requests = node.api.database.list_change_recovery_account_requests(start='', limit=100, order='by_account')['requests']
    assert len(requests) != 0


def test_list_collateralized_conversion_requests(node, wallet):
    create_account_and_fund_it(wallet, 'alice', tests=tt.Asset.Test(100), vests=tt.Asset.Test(100))
    wallet.api.convert_hive_with_collateral('alice', tt.Asset.Test(4))
    requests = node.api.database.list_collateralized_conversion_requests(start=[''], limit=100, order='by_account')['requests']
    assert len(requests) != 0


def test_list_decline_voting_rights_requests(node, wallet):
    create_account_and_fund_it(wallet, 'alice', tests=tt.Asset.Test(100), vests=tt.Asset.Test(100))
    wallet.api.decline_voting_rights('alice', True)
    requests = node.api.database.list_decline_voting_rights_requests(start='', limit=100, order='by_account')['requests']
    assert len(requests) != 0


def test_list_escrows(node, wallet):
    prepare_escrow(wallet, sender='alice')
    escrows = node.api.database.list_escrows(start=['alice', 0], limit=5, order='by_from_id')['escrows']
    assert len(escrows) != 0


def test_list_hbd_conversion_requests(wallet, node):
    create_account_and_fund_it(wallet, 'alice', tests=tt.Asset.Test(100), vests=tt.Asset.Test(100),
                               tbds=tt.Asset.Tbd(100))
    wallet.api.convert_hbd('alice', tt.Asset.Tbd(1.25))
    requests = node.api.database.list_hbd_conversion_requests(start=['alice', 0], limit=100, order='by_account')['requests']
    assert len(requests) != 0


def test_list_limit_orders(node, wallet):
    create_account_and_fund_it(wallet, 'alice', tests=tt.Asset.Test(100), vests=tt.Asset.Test(100))
    wallet.api.create_order('alice', 431, tt.Asset.Test(50), tt.Asset.Tbd(5), False, 3600)
    orders = node.api.database.list_limit_orders(start=['alice', 0], limit=100, order='by_account')['orders']
    assert len(orders) != 0


def test_list_owner_histories(node, wallet):
    create_account_and_fund_it(wallet, 'alice', tests=tt.Asset.Test(100), vests=tt.Asset.Test(100))
    # update_account_auth_key with owner parameter is called to change owner authority history
    wallet.api.update_account_auth_key('alice', 'owner', tt.Account('some key').public_key, 1)
    owner_auths = node.api.database.list_owner_histories(start=['alice', date_from_now(weeks=-1)], limit=100)['owner_auths']
    assert len(owner_auths) != 0


def test_list_savings_withdrawals(node, wallet):
    create_account_and_fund_it(wallet, 'alice', tests=tt.Asset.Test(100), vests=tt.Asset.Test(100))
    transfer_and_withdraw_from_savings(wallet, 'alice')
    withdrawals = node.api.database.list_savings_withdrawals(start=[date_from_now(weeks=-1), "alice", 0], limit=100,
                                                             order='by_complete_from_id')['withdrawals']
    assert len(withdrawals) != 0


def test_list_vesting_delegation_expirations(node, wallet):
    create_account_and_fund_it(wallet, 'alice', tests=tt.Asset.Test(100), vests=tt.Asset.Test(100))
    wallet.api.create_account('alice', 'bob', '{}')
    create_and_cancel_vesting_delegation(wallet, 'alice', 'bob')
    delegations = node.api.database.list_vesting_delegation_expirations(start=['alice', date_from_now(weeks=-1), 0],
                                                                        limit=100, order='by_account_expiration')['delegations']
    assert len(delegations) != 0


def test_list_vesting_delegations(node, wallet):
    create_account_and_fund_it(wallet, 'alice', vests=tt.Asset.Test(100))
    wallet.api.create_account('alice', 'bob', '{}')
    wallet.api.delegate_vesting_shares('alice', 'bob', tt.Asset.Vest(5))
    delegations = node.api.database.list_vesting_delegations(start=["alice", "bob"], limit=100, order='by_delegation')['delegations']
    assert len(delegations) != 0


def test_list_withdraw_vesting_routes(node, wallet):
    create_account_and_fund_it(wallet, 'alice', vests=tt.Asset.Test(100))
    wallet.api.create_account('alice', 'bob', '{}')
    wallet.api.set_withdraw_vesting_route('alice', 'bob', 15, True)
    routes = node.api.database.list_withdraw_vesting_routes(start=["alice", "bob"], limit=100, order="by_withdraw_route")['routes']
    assert len(routes) != 0


def test_list_witness_votes(node, wallet):
    create_account_and_fund_it(wallet, 'alice', vests=tt.Asset.Test(100))
    create_account_and_fund_it(wallet, 'bob', vests=tt.Asset.Test(100))

    # mark alice as new witness
    wallet.api.update_witness('alice', 'http://url.html',
                              tt.Account('alice').public_key,
                              {'account_creation_fee': tt.Asset.Test(28), 'maximum_block_size': 131072,
                               'hbd_interest_rate': 1000})
    wallet.api.vote_for_witness('bob', 'alice', True)
    votes = node.api.database.list_witness_votes(start=["alice", "bob"], limit=100, order='by_witness_account')['votes']
    assert len(votes) != 0


def test_list_witnesses(node, wallet):
    create_account_and_fund_it(wallet, 'alice', tests=tt.Asset.Test(100), vests=tt.Asset.Test(100))
    # mark alice as new witness
    wallet.api.update_witness('alice', 'http://url.html',
                              tt.Account('alice').public_key,
                              {'account_creation_fee': tt.Asset.Test(28), 'maximum_block_size': 131072,
                               'hbd_interest_rate': 1000})
    witnesses = node.api.database.list_witnesses(start='', limit=100, order='by_name')['witnesses']
    assert len(witnesses) != 0


def test_verify_authority(node, wallet):
    transaction = wallet.api.create_account('initminer', 'alice', '{}')
    node.api.database.verify_authority(trx=transaction, pack='hf26')


def test_list_proposal_votes(node, wallet):
    create_account_and_fund_it(wallet, 'alice', tests=tt.Asset.Test(100), vests=tt.Asset.Test(100),
                               tbds=tt.Asset.Tbd(300))
    create_proposal(wallet, 'alice')
    wallet.api.update_proposal_votes('alice', [0], True)
    proposal_votes = node.api.database.list_proposal_votes(start=['alice'], limit=100, order='by_voter_proposal',
                                                           order_direction='ascending', status='all')['proposal_votes']
    assert len(proposal_votes) != 0


def test_verify_signatures(node, wallet):
    transaction = wallet.api.create_account('initminer', 'alice', '{}')
    sig_digest = generate_sig_digest(transaction, tt.Account('initminer').private_key)
    node.api.database.verify_signatures(hash=sig_digest, signatures=transaction['signatures'],
                                        required_owner=[], required_active=['initminer'],
                                        required_posting=[], required_other=[])


def test_list_proposals(node, wallet):
    create_account_and_fund_it(wallet, 'alice', tests=tt.Asset.Test(100), vests=tt.Asset.Test(100),
                               tbds=tt.Asset.Tbd(300))
    create_proposal(wallet, 'alice')
    proposals = node.api.database.list_proposals(start=["alice"], limit=100, order='by_creator',
                                                 order_direction='ascending', status='all')['proposals']
    assert len(proposals) != 0
