import pytest

from test_tools import Asset, exceptions, Wallet


def test_delegated_rc_account_execute_operation(wallet: Wallet):
    accounts = []
    number_of_accounts_in_one_transaction = 10
    with wallet.in_single_transaction():
        for account_number in range(number_of_accounts_in_one_transaction):
            wallet.api.create_account('initminer', f'account-{account_number}', '{}')
            accounts.append(f'account-{account_number}')

    wallet.api.transfer_to_vesting('initminer', accounts[0], Asset.Test(0.1))
    wallet.api.delegate_rc(accounts[0], [accounts[1]], 100)
    wallet.api.create_account(accounts[1], 'alice', '{}')


def test_undelegated_rc_account_reject_execute_operation(wallet: Wallet):
    accounts = []
    number_of_accounts_in_one_transaction = 10
    with wallet.in_single_transaction():
        for account_number in range(number_of_accounts_in_one_transaction):
            wallet.api.create_account('initminer', f'account-{account_number}', '{}')
            accounts.append(f'account-{account_number}')

    wallet.api.transfer_to_vesting('initminer', accounts[0], Asset.Test(0.1))

    wallet.api.delegate_rc(accounts[0], [accounts[1]], 100)
    wallet.api.create_account(accounts[1], 'alice', '{}')

    wallet.api.delegate_rc(accounts[0], [accounts[1]], 0)

    with pytest.raises(exceptions.CommunicationError):
        wallet.api.create_account(accounts[1], 'bob', '{}')


# def test_delegations_when_delegator_lost_power(wallet: Wallet):
# #problem with withdraw vests
#     accounts = []
#     number_of_accounts_in_one_transaction = 10
#     with wallet.in_single_transaction():
#         for account_number in range(number_of_accounts_in_one_transaction):
#             wallet.api.create_account('initminer', f'account-{account_number}', '{}')
#             accounts.append(f'account-{account_number}')
#
#     wallet.api.transfer_to_vesting('initminer', accounts[0], Asset.Test(0.01))
#     state1 = wallet.api.find_rc_accounts([accounts[0]])
#     state2 = wallet.api.get_account(accounts[0])
#     number_of_rc = rc_account_info(accounts[0], 'max_rc', wallet)
#     wallet.api.delegate_rc(accounts[0], [accounts[1]], number_of_rc - 300)
#
#     assert rc_account_info(accounts[0], 'rc_manabar', wallet)['current_mana'] <= 300
#
#     state3 = wallet.api.find_rc_accounts([accounts[0]])
#     vests_to_withdraw = account_info(accounts[0], 'vesting_shares', wallet)
#     wallet.api.withdraw_vesting(accounts[0], vests_to_withdraw)
#
#     state4 = wallet.api.find_rc_accounts([accounts[0]])
#     state5 = wallet.api.get_account(accounts[0])


def test_same_value_rc_delegation(node, wallet: Wallet):
    accounts = []
    number_of_accounts_in_one_transaction = 10
    number_of_transactions = 1
    for number_of_transaction in range(number_of_transactions):
        with wallet.in_single_transaction():
            for account_number in range(number_of_accounts_in_one_transaction):
                wallet.api.create_account('initminer', f'account-{account_number}', '{}')
                accounts.append(f'account-{account_number}')

    with wallet.in_single_transaction():
        wallet.api.transfer_to_vesting('initminer', accounts[0], Asset.Test(10))
        wallet.api.transfer_to_vesting('initminer', accounts[1], Asset.Test(10))
        wallet.api.transfer_to_vesting('initminer', accounts[3], Asset.Test(10))
        wallet.api.transfer_to_vesting('initminer', accounts[4], Asset.Test(10))
        wallet.api.transfer_to_vesting('initminer', accounts[5], Asset.Test(10))

    with wallet.in_single_transaction():
        wallet.api.delegate_rc(accounts[0], [accounts[6]], 10)
        wallet.api.delegate_rc(accounts[1], [accounts[6]], 10)
        wallet.api.delegate_rc(accounts[3], [accounts[6]], 10)
        wallet.api.delegate_rc(accounts[4], [accounts[6]], 10)
        wallet.api.delegate_rc(accounts[5], [accounts[6]], 10)
    assert rc_account_info(accounts[6], 'max_rc', wallet) == 50

    with pytest.raises(exceptions.CommunicationError):
        # Can not make same delegation RC two times
        wallet.api.delegate_rc(accounts[0], [accounts[6]], 10)

    node.wait_number_of_blocks(3)

    with pytest.raises(exceptions.CommunicationError):
        # Can not make same delegation RC two times
        wallet.api.delegate_rc(accounts[0], [accounts[6]], 10)


def test_less_value_rc_delegation(wallet: Wallet):
    accounts = []
    number_of_accounts_in_one_transaction = 10
    number_of_transactions = 1
    for number_of_transaction in range(number_of_transactions):
        with wallet.in_single_transaction():
            for account_number in range(number_of_accounts_in_one_transaction):
                wallet.api.create_account('initminer', f'account-{account_number}', '{}')
                accounts.append(f'account-{account_number}')

    with wallet.in_single_transaction():
        wallet.api.transfer_to_vesting('initminer', accounts[0], Asset.Test(10))
        wallet.api.transfer_to_vesting('initminer', accounts[1], Asset.Test(10))
        wallet.api.transfer_to_vesting('initminer', accounts[3], Asset.Test(10))
        wallet.api.transfer_to_vesting('initminer', accounts[4], Asset.Test(10))
        wallet.api.transfer_to_vesting('initminer', accounts[5], Asset.Test(10))

    with wallet.in_single_transaction():
        wallet.api.delegate_rc(accounts[0], [accounts[6]], 10)
        wallet.api.delegate_rc(accounts[1], [accounts[6]], 9)
        wallet.api.delegate_rc(accounts[3], [accounts[6]], 8)
        wallet.api.delegate_rc(accounts[4], [accounts[6]], 7)
        wallet.api.delegate_rc(accounts[5], [accounts[6]], 6)
    assert rc_account_info(accounts[6], 'max_rc', wallet) == 40

    wallet.api.delegate_rc(accounts[0], [accounts[7]], 10)
    assert rc_account_info(accounts[7], 'max_rc', wallet) == 10

    wallet.api.delegate_rc(accounts[0], [accounts[7]], 5)
    assert rc_account_info(accounts[7], 'max_rc', wallet) == 5


def test_bigger_value_rc_delegation(wallet: Wallet):
    accounts = []
    number_of_accounts_in_one_transaction = 10
    number_of_transactions = 1
    for number_of_transaction in range(number_of_transactions):
        with wallet.in_single_transaction():
            for account_number in range(number_of_accounts_in_one_transaction):
                wallet.api.create_account('initminer', f'account-{account_number}', '{}')
                accounts.append(f'account-{account_number}')

    with wallet.in_single_transaction():
        wallet.api.transfer_to_vesting('initminer', accounts[0], Asset.Test(10))
        wallet.api.transfer_to_vesting('initminer', accounts[1], Asset.Test(10))
        wallet.api.transfer_to_vesting('initminer', accounts[3], Asset.Test(10))
        wallet.api.transfer_to_vesting('initminer', accounts[4], Asset.Test(10))
        wallet.api.transfer_to_vesting('initminer', accounts[5], Asset.Test(10))

    with wallet.in_single_transaction():
        wallet.api.delegate_rc(accounts[0], [accounts[6]], 6)
        wallet.api.delegate_rc(accounts[1], [accounts[6]], 7)
        wallet.api.delegate_rc(accounts[3], [accounts[6]], 8)
        wallet.api.delegate_rc(accounts[4], [accounts[6]], 9)
        wallet.api.delegate_rc(accounts[5], [accounts[6]], 10)
    assert rc_account_info(accounts[6], 'max_rc', wallet) == 40

    wallet.api.delegate_rc(accounts[0], [accounts[7]], 5)
    assert rc_account_info(accounts[7], 'max_rc', wallet) == 5

    wallet.api.delegate_rc(accounts[0], [accounts[7]], 10)
    assert rc_account_info(accounts[7], 'max_rc', wallet) == 10


def test_large_rc_delegation(node, wallet: Wallet):
#probably BUG
    accounts = []
    number_of_accounts_in_one_transaction = 10
    number_of_transactions = 1
    for number_of_transaction in range(number_of_transactions):
        with wallet.in_single_transaction():
            for account_number in range(number_of_accounts_in_one_transaction):
                wallet.api.create_account('initminer', f'account-{account_number}', '{}')
                accounts.append(f'account-{account_number}')

    wallet.api.transfer_to_vesting('initminer', accounts[0], Asset.Test(200000000))
    rc_to_delegate = int(rc_account_info(accounts[0], 'rc_manabar', wallet)['current_mana']) - 11100
    x = node.api.rc.get_resource_pool()
    y = node.api.rc.get_resource_params()
    wallet.api.delegate_rc(accounts[0], [accounts[1]], rc_to_delegate)
    assert int(rc_account_info(accounts[1], 'max_rc', wallet)) == rc_to_delegate


def test_out_of_int64_rc_delegation(wallet: Wallet):
#uncorrect error message
    accounts = []
    number_of_accounts_in_one_transaction = 10
    number_of_transactions = 1
    for number_of_transaction in range(number_of_transactions):
        with wallet.in_single_transaction():
            for account_number in range(number_of_accounts_in_one_transaction):
                wallet.api.create_account('initminer', f'account-{account_number}', '{}')
                accounts.append(f'account-{account_number}')

    wallet.api.transfer_to_vesting('initminer', accounts[0], Asset.Test(2000))

    with pytest.raises(exceptions.CommunicationError):
        wallet.api.delegate_rc(accounts[0], [accounts[1]], 9223372036854775808)


def test_delegations_rc_to_one_receiver(wallet: Wallet):
    accounts = []
    number_of_accounts = 120

    with wallet.in_single_transaction():
        for account_number in range(number_of_accounts):
            wallet.api.create_account('initminer', f'account-{account_number}', '{}')
            accounts.append(f'account-{account_number}')

    number_of_transfers = 100
    account_number_absolute = 1
    with wallet.in_single_transaction():
        for transfer_number in range(number_of_transfers):
            wallet.api.transfer_to_vesting('initminer', accounts[account_number_absolute], Asset.Test(100000))
            wallet.api.transfer('initminer', accounts[account_number_absolute], Asset.Test(10), '')
            account_number_absolute = account_number_absolute + 1

    number_of_delegations = 100
    account_number_absolute = 1
    with wallet.in_single_transaction():
        for delegation_number in range(number_of_delegations):
            wallet.api.delegate_rc(accounts[account_number_absolute], [accounts[0]], 10)
            account_number_absolute = account_number_absolute + 1


def test_reject_of_delegation_of_delegated_rc(wallet: Wallet):
    accounts = []
    number_of_accounts = 10

    with wallet.in_single_transaction():
        for account_number in range(number_of_accounts):
            wallet.api.create_account('initminer', f'account-{account_number}', '{}')
            accounts.append(f'account-{account_number}')

    wallet.api.transfer_to_vesting('initminer', accounts[0], Asset.Test(0.1))
    wallet.api.delegate_rc(accounts[0], [accounts[1]], 100)

    with pytest.raises(exceptions.CommunicationError):
        wallet.api.delegate_rc(accounts[1], [accounts[2]], 50)


def test_wrong_sign_in_transaction(wallet: Wallet):
    accounts = []
    number_of_accounts_in_one_transaction = 10
    number_of_transactions = 1
    for number_of_transaction in range(number_of_transactions):
        with wallet.in_single_transaction():
            for account_number in range(number_of_accounts_in_one_transaction):
                wallet.api.create_account('initminer', f'account-{account_number}', '{}')
                accounts.append(f'account-{account_number}')

    wallet.api.transfer_to_vesting('initminer', accounts[0], Asset.Test(10))

    x = wallet.api.delegate_rc(accounts[0], [accounts[1]], 100, broadcast=False)
    x['operations'][0][1]['required_posting_auths'][0] = accounts[1]

    with pytest.raises(exceptions.CommunicationError):
        wallet.api.sign_transaction(x)


def test_minus_rc_delegation(wallet: Wallet):
    accounts = []
    number_of_accounts_in_one_transaction = 10
    number_of_transactions = 1
    for number_of_transaction in range(number_of_transactions):
        with wallet.in_single_transaction():
            for account_number in range(number_of_accounts_in_one_transaction):
                wallet.api.create_account('initminer', f'account-{account_number}', '{}')
                accounts.append(f'account-{account_number}')

    wallet.api.transfer_to_vesting('initminer', accounts[0], Asset.Test(10))
    with pytest.raises(exceptions.CommunicationError):
        wallet.api.delegate_rc(accounts[0], [accounts[1]], -100)


def test_power_up_delegator(wallet: Wallet):
    accounts = []
    number_of_accounts_in_one_transaction = 10
    number_of_transactions = 1
    for number_of_transaction in range(number_of_transactions):
        with wallet.in_single_transaction():
            for account_number in range(number_of_accounts_in_one_transaction):
                wallet.api.create_account('initminer', f'account-{account_number}', '{}')
                accounts.append(f'account-{account_number}')

    wallet.api.transfer_to_vesting('initminer', accounts[0], Asset.Test(10))
    wallet.api.delegate_rc(accounts[0], [accounts[1]], 100)
    rc0 = rc_account_info(accounts[1], 'rc_manabar', wallet)['current_mana']
    wallet.api.transfer_to_vesting('initminer', accounts[0], Asset.Test(100))
    rc1 = rc_account_info(accounts[1], 'rc_manabar', wallet)['current_mana']
    assert rc0 == rc1


def test_multidelegation(wallet: Wallet):
    accounts = []
    number_of_accounts_in_one_transaction = 100
    number_of_transactions = 1
    for number_of_transaction in range(number_of_transactions):
        with wallet.in_single_transaction():
            for account_number in range(number_of_accounts_in_one_transaction):
                wallet.api.create_account('initminer', f'account-{account_number}', '{}')
                accounts.append(f'account-{account_number}')

    wallet.api.transfer_to_vesting('initminer', accounts[0], Asset.Test(1000))
    wallet.api.delegate_rc(accounts[0], accounts[1:number_of_accounts_in_one_transaction], 5)


def rc_account_info(account, name_of_data, wallet):
    data_set = wallet.api.find_rc_accounts([account])[0]
    specyfic_data = data_set[name_of_data]
    return specyfic_data


def account_info(account, name_of_data, wallet):
    data_set = wallet.api.get_account(account)
    specyfic_data = data_set[name_of_data]
    return specyfic_data
