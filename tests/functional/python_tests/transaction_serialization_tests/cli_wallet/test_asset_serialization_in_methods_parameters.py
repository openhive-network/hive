import pytest

import test_tools as tt

from .....local_tools import create_account_and_fund_it, date_from_now
from .local_tools import run_for_all_cases, create_alice_and_bob_accounts_with_received_rewards


@pytest.mark.testnet
@run_for_all_cases(transfer_amount=tt.Asset.Test(100))
def test_transfer_of_hives(prepared_wallet, transfer_amount):
    prepared_wallet.api.create_account('initminer', 'alice', '{}')

    prepared_wallet.api.transfer('initminer', 'alice', transfer_amount, 'memo')


@pytest.mark.testnet
@run_for_all_cases(transfer_amount=tt.Asset.Tbd(100))
def test_transfer_of_hbds(prepared_wallet, transfer_amount):
    prepared_wallet.api.create_account('initminer', 'alice', '{}')

    prepared_wallet.api.transfer('initminer', 'alice', transfer_amount, 'memo')


@pytest.mark.testnet
@run_for_all_cases(transfer_amount=tt.Asset.Test(100))
def test_transfer_nonblocking_of_hives(prepared_wallet, transfer_amount):
    prepared_wallet.api.create_account('initminer', 'alice', '{}')

    prepared_wallet.api.transfer_nonblocking('initminer', 'alice', transfer_amount, 'memo')


@pytest.mark.testnet
@run_for_all_cases(transfer_amount=tt.Asset.Tbd(100))
def test_transfer_nonblocking_of_hbds(prepared_wallet, transfer_amount):
    prepared_wallet.api.create_account('initminer', 'alice', '{}')

    prepared_wallet.api.transfer_nonblocking('initminer', 'alice', transfer_amount, 'memo')


@pytest.mark.testnet
@run_for_all_cases(escrow_tbd_amount=tt.Asset.Tbd(1), escrow_tests_amount=tt.Asset.Test(2))
def test_escrow_transfer(prepared_wallet, escrow_tbd_amount, escrow_tests_amount):
    with prepared_wallet.in_single_transaction():
        prepared_wallet.api.create_account('initminer', 'alice', '{}')
        prepared_wallet.api.create_account('initminer', 'bob', '{}')

    prepared_wallet.api.escrow_transfer('initminer', 'alice', 'bob', 99, escrow_tbd_amount, escrow_tests_amount,
                                        escrow_tbd_amount, date_from_now(weeks=16), date_from_now(weeks=20), '{}')


@pytest.mark.testnet
@run_for_all_cases(escrow_tbd_amount=tt.Asset.Tbd(1), escrow_tests_amount=tt.Asset.Test(2))
def test_escrow_release(prepared_wallet, escrow_tbd_amount, escrow_tests_amount):

    create_account_and_fund_it(prepared_wallet, 'alice', vests=tt.Asset.Test(50))
    create_account_and_fund_it(prepared_wallet, 'bob', vests=tt.Asset.Test(50))

    prepared_wallet.api.escrow_transfer('initminer', 'alice', 'bob', 99, tt.Asset.Tbd(1), tt.Asset.Test(2),
                                        tt.Asset.Tbd(1), date_from_now(weeks=16), date_from_now(weeks=20), '{}')

    prepared_wallet.api.escrow_approve('initminer', 'alice', 'bob', 'bob', 99, True)

    prepared_wallet.api.escrow_approve('initminer', 'alice', 'bob', 'alice', 99, True)

    prepared_wallet.api.escrow_dispute('initminer', 'alice', 'bob', 'initminer', 99)

    prepared_wallet.api.escrow_release('initminer', 'alice', 'bob', 'bob', 'alice', 99, escrow_tbd_amount,
                                       escrow_tests_amount)


@pytest.mark.testnet
@run_for_all_cases(claim_test_amount=tt.Asset.Test(0))
def test_claim_account_creation(node, prepared_wallet, claim_test_amount):
    node.wait_number_of_blocks(18)
    prepared_wallet.api.claim_account_creation('initminer', claim_test_amount)


@pytest.mark.testnet
@run_for_all_cases(claim_test_amount=tt.Asset.Test(0))
def test_claim_account_creation_nonblocking(node, prepared_wallet, claim_test_amount):
    node.wait_number_of_blocks(18)
    prepared_wallet.api.claim_account_creation_nonblocking('initminer', claim_test_amount)


@pytest.mark.testnet
@run_for_all_cases(create_account_test_amount=tt.Asset.Test(2))
def test_create_funded_account_with_keys(node, prepared_wallet, create_account_test_amount):
    key = 'TST8grZpsMPnH7sxbMVZHWEu1D26F3GwLW1fYnZEuwzT4Rtd57AER'
    prepared_wallet.api.create_funded_account_with_keys('initminer', 'alice', create_account_test_amount, 'memo', '{}',
                                                        key, key, key, key)


@pytest.mark.testnet
@run_for_all_cases(delegate_vest_amount=tt.Asset.Vest(1))
def test_delegate_vesting_shares(node, prepared_wallet, delegate_vest_amount):
    create_account_and_fund_it(prepared_wallet, 'alice', vests=tt.Asset.Test(50))
    prepared_wallet.api.create_account('initminer', 'bob', '{}')

    prepared_wallet.api.delegate_vesting_shares('alice', 'bob', delegate_vest_amount)


@pytest.mark.testnet
@run_for_all_cases(delegate_vest_amount=tt.Asset.Vest(1))
def test_delegate_vesting_shares_nonblocking(node, prepared_wallet, delegate_vest_amount):
    create_account_and_fund_it(prepared_wallet, 'alice', vests=tt.Asset.Test(50))
    prepared_wallet.api.create_account('initminer', 'bob', '{}')

    prepared_wallet.api.delegate_vesting_shares_nonblocking('alice', 'bob', delegate_vest_amount)


@pytest.mark.testnet
@run_for_all_cases(delegate_vest_amount=tt.Asset.Vest(1), transfer_test_amount=tt.Asset.Test(6))
def test_delegate_vesting_shares_and_transfer(node, prepared_wallet, delegate_vest_amount, transfer_test_amount):
    create_account_and_fund_it(prepared_wallet, 'alice', tests=tt.Asset.Test(50), vests=tt.Asset.Test(50))
    prepared_wallet.api.create_account('initminer', 'bob', '{}')

    prepared_wallet.api.delegate_vesting_shares_and_transfer('alice', 'bob', delegate_vest_amount, transfer_test_amount,
                                                             'transfer_memo')


@pytest.mark.testnet
@run_for_all_cases(delegate_vest_amount=tt.Asset.Vest(1), transfer_test_amount=tt.Asset.Test(6))
def test_delegate_vesting_shares_and_transfer_nonblocking(node, prepared_wallet, delegate_vest_amount,
                                                                  transfer_test_amount):
    create_account_and_fund_it(prepared_wallet, 'alice', tests=tt.Asset.Test(50), vests=tt.Asset.Test(50))
    prepared_wallet.api.create_account('initminer', 'bob', '{}')

    prepared_wallet.api.delegate_vesting_shares_and_transfer_nonblocking('alice', 'bob', delegate_vest_amount,
                                                                         transfer_test_amount, 'transfer_memo')


@pytest.mark.testnet
@run_for_all_cases(transfer_test_amount=tt.Asset.Test(100))
def test_transfer_to_vesting(prepared_wallet, transfer_test_amount):
    prepared_wallet.api.create_account('initminer', 'alice', '{}')

    prepared_wallet.api.transfer_to_vesting('initminer', 'alice', transfer_test_amount)


@pytest.mark.testnet
@run_for_all_cases(transfer_test_amount=tt.Asset.Test(100))
def test_transfer_to_vesting_nonblocking(prepared_wallet, transfer_test_amount):
    prepared_wallet.api.create_account('initminer', 'alice', '{}')

    prepared_wallet.api.transfer_to_vesting_nonblocking('initminer', 'alice', transfer_test_amount)


@pytest.mark.testnet
@run_for_all_cases(transfer_test_amount=tt.Asset.Test(100))
def test_transfer_to_savings_tests(prepared_wallet, transfer_test_amount):
    prepared_wallet.api.create_account('initminer', 'alice', '{}')

    prepared_wallet.api.transfer_to_savings('initminer', 'alice', transfer_test_amount, 'memo')


@pytest.mark.testnet
@run_for_all_cases(transfer_tbd_amount=tt.Asset.Tbd(100))
def test_transfer_to_savings_tbds(prepared_wallet, transfer_tbd_amount):
    prepared_wallet.api.create_account('initminer', 'alice', '{}')

    prepared_wallet.api.transfer_to_savings('initminer', 'alice', transfer_tbd_amount, 'memo')


@pytest.mark.testnet
@run_for_all_cases(transfer_test_amount=tt.Asset.Test(1))
def test_transfer_from_savings_tests(prepared_wallet, transfer_test_amount):
    create_account_and_fund_it(prepared_wallet, 'alice', vests=tt.Asset.Test(10))
    prepared_wallet.api.create_account('initminer', 'bob', '{}')

    with prepared_wallet.in_single_transaction():
        prepared_wallet.api.transfer_to_savings('initminer', 'alice', tt.Asset.Test(10), 'memo')
        prepared_wallet.api.transfer_to_savings('initminer', 'alice', tt.Asset.Tbd(10), 'memo')

    prepared_wallet.api.transfer_from_savings('alice', 1000, 'bob', transfer_test_amount, 'memo')


@pytest.mark.testnet
@run_for_all_cases(transfer_tbd_amount=tt.Asset.Tbd(1))
def test_transfer_from_savings_tbds(prepared_wallet, transfer_tbd_amount):
    create_account_and_fund_it(prepared_wallet, 'alice', vests=tt.Asset.Test(10))
    prepared_wallet.api.create_account('initminer', 'bob', '{}')

    with prepared_wallet.in_single_transaction():
        prepared_wallet.api.transfer_to_savings('initminer', 'alice', tt.Asset.Test(10), 'memo')
        prepared_wallet.api.transfer_to_savings('initminer', 'alice', tt.Asset.Tbd(10), 'memo')

    prepared_wallet.api.transfer_from_savings('alice', 1000, 'bob', transfer_tbd_amount, 'memo')


@pytest.mark.testnet
@run_for_all_cases(withdraw_vests_amount=tt.Asset.Vest(10))
def test_withdraw_vesting(prepared_wallet, withdraw_vests_amount):
    create_account_and_fund_it(prepared_wallet, 'alice', vests=tt.Asset.Test(500000))

    prepared_wallet.api.withdraw_vesting('alice', withdraw_vests_amount)


@pytest.mark.testnet
@run_for_all_cases(convert_test_amount=tt.Asset.Test(100))
def test_convert_hive_with_collateral(prepared_wallet, convert_test_amount):
    create_account_and_fund_it(prepared_wallet, 'alice', tests=tt.Asset.Test(100), vests=tt.Asset.Test(100))

    prepared_wallet.api.convert_hive_with_collateral('alice', convert_test_amount)


@pytest.mark.testnet
@run_for_all_cases(convert_tbd_amount=tt.Asset.Tbd(10))
def test_convert_hbd(prepared_wallet, convert_tbd_amount):
    create_account_and_fund_it(prepared_wallet, 'alice', tests=tt.Asset.Test(100), vests=tt.Asset.Test(100))

    prepared_wallet.api.convert_hive_with_collateral('alice', tt.Asset.Test(100))
    prepared_wallet.api.convert_hbd('alice', convert_tbd_amount)


@pytest.mark.testnet
@run_for_all_cases(estimate_hive_amount=tt.Asset.Test(100))
def test_estimate_hive_collateral(prepared_wallet, estimate_hive_amount):
    prepared_wallet.api.estimate_hive_collateral(estimate_hive_amount)



@pytest.mark.testnet
@run_for_all_cases(create_order_tests_amount=tt.Asset.Test(100), create_order_tbds_amount=tt.Asset.Tbd(100))
def test_create_order(prepared_wallet, create_order_tests_amount, create_order_tbds_amount):
    create_account_and_fund_it(prepared_wallet, 'alice', tests=tt.Asset.Test(100), vests=tt.Asset.Test(100))

    prepared_wallet.api.create_order('alice', 1000, create_order_tests_amount, create_order_tbds_amount, False, 1000)


@pytest.mark.testnet
@run_for_all_cases(create_proposal_tbds_amount=tt.Asset.Tbd(1000000))
def test_create_proposal(prepared_wallet, create_proposal_tbds_amount):
    create_account_and_fund_it(prepared_wallet, 'alice', tbds=tt.Asset.Tbd(1000000), vests=tt.Asset.Test(1000000))

    prepared_wallet.api.post_comment('alice', 'permlink', '', 'parent-permlink', 'title', 'body', '{}')

    prepared_wallet.api.create_proposal('alice', 'alice', date_from_now(weeks=16), date_from_now(weeks=20),
                                        create_proposal_tbds_amount, 'subject-1', 'permlink')


@pytest.mark.testnet
@run_for_all_cases(update_proposal_tbd_amount=tt.Asset.Tbd(10))
def test_update_proposal(prepared_wallet, update_proposal_tbd_amount):
    create_account_and_fund_it(prepared_wallet, 'alice', tbds=tt.Asset.Tbd(1000000), vests=tt.Asset.Test(1000000))

    prepared_wallet.api.post_comment('alice', 'permlink', '', 'parent-permlink', 'title', 'body', '{}')

    prepared_wallet.api.create_proposal('alice', 'alice', date_from_now(weeks=16), date_from_now(weeks=20),
                                        tt.Asset.Tbd(1000000), 'subject-1', 'permlink')

    prepared_wallet.api.update_proposal(0, 'alice', update_proposal_tbd_amount, 'subject-1', 'permlink',
                                        date_from_now(weeks=19))


@pytest.mark.testnet
@run_for_all_cases(transfer_test_amount=tt.Asset.Test(1000000))
def test_recurrent_transfer_matched(prepared_wallet, transfer_test_amount):
    create_account_and_fund_it(prepared_wallet, 'alice', tests=tt.Asset.Test(1000000), vests=tt.Asset.Test(1000000))
    prepared_wallet.api.create_account('initminer', 'bob', '{}')

    prepared_wallet.api.recurrent_transfer('alice', 'bob', transfer_test_amount, 'memo', 100, 10)


"""
Claim_reward_balance allows you to transfer funds from your reward balance to your regular balance. In order to have
funds in the reward balance, you must receive a reward. This takes 1 hour on the test network. Because of this,
'faketime' was used and the time in 'node' was changed one hour forward.
"""


@pytest.mark.testnet
@run_for_all_cases(claim_test_amount=tt.Asset.Test(0), claim_tbd_amount=tt.Asset.Tbd(0.01),
                   claim_vest_amount=tt.Asset.Vest(0.01))
def test_claim_reward_balance_matched(node, prepared_wallet, claim_test_amount, claim_tbd_amount, claim_vest_amount):
    create_alice_and_bob_accounts_with_received_rewards(node, prepared_wallet)

    prepared_wallet.api.claim_reward_balance('alice', claim_test_amount, claim_tbd_amount, claim_vest_amount)
