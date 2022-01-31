from concurrent.futures import ThreadPoolExecutor

import pytest

from test_tools import Asset, exceptions, logger, Wallet


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


def test_rc_delegation_to_same_receiver(node, wallet: Wallet):
    accounts = create_accounts(6, wallet)

    with wallet.in_single_transaction():
        wallet.api.transfer_to_vesting('initminer', accounts[0], Asset.Test(10))
        wallet.api.transfer_to_vesting('initminer', accounts[1], Asset.Test(10))
        wallet.api.transfer_to_vesting('initminer', accounts[2], Asset.Test(10))
        wallet.api.transfer_to_vesting('initminer', accounts[3], Asset.Test(10))
        wallet.api.transfer_to_vesting('initminer', accounts[4], Asset.Test(10))

    with wallet.in_single_transaction():
        wallet.api.delegate_rc(accounts[0], [accounts[5]], 10)
        wallet.api.delegate_rc(accounts[1], [accounts[5]], 2)
        wallet.api.delegate_rc(accounts[2], [accounts[5]], 42)
        wallet.api.delegate_rc(accounts[3], [accounts[5]], 13)
        wallet.api.delegate_rc(accounts[4], [accounts[5]], 5)
    assert get_rc_account_info(accounts[5], wallet)['max_rc'] == 72


def test_same_rc_delegation_rejection(node, wallet: Wallet):
    accounts = create_accounts(2, wallet)

    wallet.api.transfer_to_vesting('initminer', accounts[0], Asset.Test(10))
    wallet.api.delegate_rc(accounts[0], [accounts[1]], 10)

    with pytest.raises(exceptions.CommunicationError):
        # Can not make same delegation RC two times
        wallet.api.delegate_rc(accounts[0], [accounts[1]], 10)


def test_overwriting_of_delegated_rc_value(wallet: Wallet):
    accounts = create_accounts(2, wallet)

    wallet.api.transfer_to_vesting('initminer', accounts[0], Asset.Test(10))

    wallet.api.delegate_rc(accounts[0], [accounts[1]], 5)
    assert get_rc_account_info(accounts[1], wallet)['max_rc'] == 5

    wallet.api.delegate_rc(accounts[0], [accounts[1]], 10)
    assert get_rc_account_info(accounts[1], wallet)['max_rc'] == 10

    wallet.api.delegate_rc(accounts[0], [accounts[1]], 7)
    assert get_rc_account_info(accounts[1], wallet)['max_rc'] == 7


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


@pytest.mark.node_shared_file_size('16G')
def test_multidelegation(wallet: Wallet):

    number_of_accounts = 100000
    logger.info('Start of account creation')
    accounts = get_accounts_name(wallet.create_accounts(number_of_accounts, 'receiver'))
    accounts_to_delegate = split_list(accounts, int(number_of_accounts / 100))
    logger.info('End of account creation')

    number_of_threads = 50
    delegators = []

    with wallet.in_single_transaction():
        for thread_number in range(number_of_threads):
            wallet.api.create_account('initminer', f'delegator{thread_number}', '{}')
            delegators.append(f'delegator{thread_number}')

    with wallet.in_single_transaction():
        for thread_number in range(number_of_threads):
            wallet.api.transfer_to_vesting('initminer', f'delegator{thread_number}', Asset.Test(0.1))


    accounts_to_delegate_packs = []
    for thread_number in range(number_of_threads + 1):
        accounts_to_delegate_packs.append(int(thread_number / number_of_threads * len(accounts_to_delegate)))

    tasks_list = []
    executor = ThreadPoolExecutor(max_workers=number_of_threads)
    for thread_number in range(number_of_threads):
        tasks_list.append(executor.submit(delegation_rc, delegators[thread_number], wallet,
                                          accounts_to_delegate_packs[thread_number],
                                          accounts_to_delegate_packs[thread_number + 1],
                                          accounts_to_delegate, thread_number))

    for thread_number in tasks_list:
        thread_number.result()

    assert get_rc_account_info(accounts[0], wallet)['received_delegated_rc'] == 1
    assert get_rc_account_info(accounts[-1], wallet)['received_delegated_rc'] == 1
    assert get_rc_account_info(accounts[int(len(accounts)/2)], wallet)['received_delegated_rc'] == 1


def delegation_rc(creator, wallet, first_accounts_pack, last_accounts_pack, accounts_to_delegate, thread_number):
    logger.info(f'Delegation thread {thread_number} work START')
    for number_of_account_pack in range(first_accounts_pack, last_accounts_pack):
        wallet.api.delegate_rc(creator, accounts_to_delegate[number_of_account_pack], 1)
    logger.info(f'Delegation thread {thread_number} work END')

def get_accounts_name(accounts):
    accounts_names = []
    for account_number in range(len(accounts)):
        accounts_names.append(accounts[account_number].name)
    return accounts_names

def split_list(alist, wanted_parts):
    length = len(alist)
    return [alist[i * length // wanted_parts: (i + 1) * length // wanted_parts]
            for i in range(wanted_parts)]


def get_rc_account_info(account, wallet):
    return wallet.api.find_rc_accounts([account])[0]


def create_accounts(number_of_accounts, wallet):
    accounts = []
    with wallet.in_single_transaction():
        for account_number in range(number_of_accounts):
            wallet.api.create_account('initminer', f'account-{account_number}', '{}')
            accounts.append(f'account-{account_number}')
    return accounts
