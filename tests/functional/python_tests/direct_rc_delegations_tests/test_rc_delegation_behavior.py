import pytest

from test_tools import Asset, exceptions, Wallet


def test_delegated_rc_account_execute_operation(wallet: Wallet):
    accounts = create_accounts(2, wallet)

    wallet.api.transfer_to_vesting('initminer', accounts[0], Asset.Test(0.1))
    wallet.api.delegate_rc(accounts[0], [accounts[1]], 100)
    wallet.api.create_account(accounts[1], 'alice', '{}')


def test_undelegated_rc_account_reject_execute_operation(wallet: Wallet):
    accounts = create_accounts(2, wallet)

    wallet.api.transfer_to_vesting('initminer', accounts[0], Asset.Test(0.1))

    wallet.api.delegate_rc(accounts[0], [accounts[1]], 100)
    wallet.api.create_account(accounts[1], 'alice', '{}')

    wallet.api.delegate_rc(accounts[0], [accounts[1]], 0)

    with pytest.raises(exceptions.CommunicationError):
        wallet.api.create_account(accounts[1], 'bob', '{}')


def test_same_value_rc_delegation(node, wallet: Wallet):
    accounts = create_accounts(6, wallet)

    with wallet.in_single_transaction():
        wallet.api.transfer_to_vesting('initminer', accounts[0], Asset.Test(10))
        wallet.api.transfer_to_vesting('initminer', accounts[1], Asset.Test(10))
        wallet.api.transfer_to_vesting('initminer', accounts[2], Asset.Test(10))
        wallet.api.transfer_to_vesting('initminer', accounts[3], Asset.Test(10))
        wallet.api.transfer_to_vesting('initminer', accounts[4], Asset.Test(10))

    with wallet.in_single_transaction():
        wallet.api.delegate_rc(accounts[0], [accounts[5]], 10)
        wallet.api.delegate_rc(accounts[1], [accounts[5]], 10)
        wallet.api.delegate_rc(accounts[2], [accounts[5]], 10)
        wallet.api.delegate_rc(accounts[3], [accounts[5]], 10)
        wallet.api.delegate_rc(accounts[4], [accounts[5]], 10)
    assert get_rc_account_info(accounts[5], wallet)['max_rc'] == 50

    with pytest.raises(exceptions.CommunicationError):
        # Can not make same delegation RC two times
        wallet.api.delegate_rc(accounts[0], [accounts[5]], 10)

    node.wait_number_of_blocks(3)

    with pytest.raises(exceptions.CommunicationError):
        # Can not make same delegation RC two times
        wallet.api.delegate_rc(accounts[0], [accounts[5]], 10)


def test_less_value_rc_delegation(wallet: Wallet):
    accounts = create_accounts(6, wallet)

    with wallet.in_single_transaction():
        wallet.api.transfer_to_vesting('initminer', accounts[0], Asset.Test(10))
        wallet.api.transfer_to_vesting('initminer', accounts[1], Asset.Test(10))
        wallet.api.transfer_to_vesting('initminer', accounts[2], Asset.Test(10))
        wallet.api.transfer_to_vesting('initminer', accounts[3], Asset.Test(10))
        wallet.api.transfer_to_vesting('initminer', accounts[4], Asset.Test(10))

    with wallet.in_single_transaction():
        wallet.api.delegate_rc(accounts[0], [accounts[5]], 10)
        wallet.api.delegate_rc(accounts[1], [accounts[5]], 9)
        wallet.api.delegate_rc(accounts[2], [accounts[5]], 8)
        wallet.api.delegate_rc(accounts[3], [accounts[5]], 7)
        wallet.api.delegate_rc(accounts[4], [accounts[5]], 6)
    assert get_rc_account_info(accounts[5], wallet)['max_rc'] == 40


def test_bigger_value_rc_delegation(wallet: Wallet):
    accounts = create_accounts(6, wallet)

    with wallet.in_single_transaction():
        wallet.api.transfer_to_vesting('initminer', accounts[0], Asset.Test(10))
        wallet.api.transfer_to_vesting('initminer', accounts[1], Asset.Test(10))
        wallet.api.transfer_to_vesting('initminer', accounts[2], Asset.Test(10))
        wallet.api.transfer_to_vesting('initminer', accounts[3], Asset.Test(10))
        wallet.api.transfer_to_vesting('initminer', accounts[4], Asset.Test(10))

    with wallet.in_single_transaction():
        wallet.api.delegate_rc(accounts[0], [accounts[5]], 6)
        wallet.api.delegate_rc(accounts[1], [accounts[5]], 7)
        wallet.api.delegate_rc(accounts[2], [accounts[5]], 8)
        wallet.api.delegate_rc(accounts[3], [accounts[5]], 9)
        wallet.api.delegate_rc(accounts[4], [accounts[5]], 10)
    assert get_rc_account_info(accounts[5], wallet)['max_rc'] == 40


def test_overwriting_of_transactions(wallet:Wallet):
    accounts = create_accounts(2, wallet)

    wallet.api.transfer_to_vesting('initminer', accounts[0], Asset.Test(10))

    wallet.api.delegate_rc(accounts[0], [accounts[1]], 5)
    assert get_rc_account_info(accounts[1], wallet)['max_rc'] == 5

    wallet.api.delegate_rc(accounts[0], [accounts[1]], 10)
    assert get_rc_account_info(accounts[1], wallet)['max_rc'] == 10

    wallet.api.delegate_rc(accounts[0], [accounts[1]], 15)
    assert get_rc_account_info(accounts[1], wallet)['max_rc'] == 15


def test_large_rc_delegation(node, wallet: Wallet):
    # Wait for block is necessary because transactions must always enter the same specific blocks.
    # This way, 'cost of transaction' is always same and is possible to delegate maximal, huge amount of RC.
    accounts = create_accounts(2, wallet)

    cost_of_transaction = 11100
    node.wait_for_block_with_number(3)
    wallet.api.transfer_to_vesting('initminer', accounts[0], Asset.Test(200000000))
    rc_to_delegate = int(get_rc_account_info(accounts[0], wallet)['rc_manabar']['current_mana']) - cost_of_transaction
    wallet.api.delegate_rc(accounts[0], [accounts[1]], rc_to_delegate)
    assert int(get_rc_account_info(accounts[1], wallet)['max_rc']) == rc_to_delegate


def test_out_of_int64_rc_delegation(wallet: Wallet):
    # uncorrect error message
    accounts = create_accounts(2, wallet)

    wallet.api.transfer_to_vesting('initminer', accounts[0], Asset.Test(2000))

    with pytest.raises(exceptions.CommunicationError):
        wallet.api.delegate_rc(accounts[0], [accounts[1]], 9223372036854775808)


def test_delegations_rc_to_one_receiver(wallet: Wallet):
    accounts = create_accounts(700, wallet)

    with wallet.in_single_transaction():
        for account in accounts[1:]:
            wallet.api.transfer_to_vesting('initminer', account, Asset.Test(100000))
            wallet.api.transfer('initminer', account, Asset.Test(10), '')

    with wallet.in_single_transaction():
        for account in accounts[1:]:
            wallet.api.delegate_rc(account, [accounts[0]], 10)


def test_reject_of_delegation_of_delegated_rc(wallet: Wallet):
    accounts = create_accounts(3, wallet)

    wallet.api.transfer_to_vesting('initminer', accounts[0], Asset.Test(0.1))
    wallet.api.delegate_rc(accounts[0], [accounts[1]], 100)

    with pytest.raises(exceptions.CommunicationError):
        wallet.api.delegate_rc(accounts[1], [accounts[2]], 50)


def test_wrong_sign_in_transaction(wallet: Wallet):
    accounts = create_accounts(2, wallet)

    wallet.api.transfer_to_vesting('initminer', accounts[0], Asset.Test(10))

    operation = wallet.api.delegate_rc(accounts[0], [accounts[1]], 100, broadcast=False)
    operation['operations'][0][1]['required_posting_auths'][0] = accounts[1]

    with pytest.raises(exceptions.CommunicationError):
        wallet.api.sign_transaction(operation)


def test_minus_rc_delegation(wallet: Wallet):
    accounts = create_accounts(2, wallet)

    wallet.api.transfer_to_vesting('initminer', accounts[0], Asset.Test(10))
    with pytest.raises(exceptions.CommunicationError):
        wallet.api.delegate_rc(accounts[0], [accounts[1]], -100)


def test_power_up_delegator(wallet: Wallet):
    accounts = create_accounts(2, wallet)

    wallet.api.transfer_to_vesting('initminer', accounts[0], Asset.Test(10))
    wallet.api.delegate_rc(accounts[0], [accounts[1]], 100)
    rc0 = get_rc_account_info(accounts[1], wallet)['rc_manabar']['current_mana']
    wallet.api.transfer_to_vesting('initminer', accounts[0], Asset.Test(100))
    rc1 = get_rc_account_info(accounts[1], wallet)['rc_manabar']['current_mana']
    assert rc0 == rc1


def test_multidelegation(wallet: Wallet):
    number_of_aacounts = 100
    accounts = create_accounts(number_of_aacounts, wallet)

    wallet.api.transfer_to_vesting('initminer', accounts[0], Asset.Test(1000))
    wallet.api.delegate_rc(accounts[0], accounts[1:number_of_aacounts], 5)


def get_rc_account_info(account, wallet):
    return wallet.api.find_rc_accounts([account])[0]

def create_accounts(number_of_accounts, wallet):
    accounts = []
    with wallet.in_single_transaction():
        for account_number in range(number_of_accounts):
            wallet.api.create_account('initminer', f'account-{account_number}', '{}')
            accounts.append(f'account-{account_number}')
    return accounts
