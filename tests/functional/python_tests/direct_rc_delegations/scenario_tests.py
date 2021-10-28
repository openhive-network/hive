from concurrent.futures import ThreadPoolExecutor

import pytest

from test_tools import Account, constants, exceptions, logger, Wallet
import time


def test_delegations_without_vests(world):
    node = world.create_init_node()
    node.config.shared_file_size = '32G'
    node.set_clean_up_policy(constants.NodeCleanUpPolicy.DO_NOT_REMOVE_FILES)
    node.run()
    wallet = Wallet(attach_to=node)

    accounts = []
    number_of_accounts_in_one_transaction = 100
    number_of_transactions = 1
    number_of_accounts = number_of_accounts_in_one_transaction * number_of_transactions
    account_number_absolute = 0

    # created_accounts = Account.create_multiple(number_of_accounts, 'account')
    for number_of_transaction in range(number_of_transactions):
        with wallet.in_single_transaction():
            for account_number in range(number_of_accounts_in_one_transaction):
                wallet.api.create_account('initminer', f'account-{account_number_absolute}', '{}')
                accounts.append(f'account-{account_number_absolute}')
                account_number_absolute = account_number_absolute + 1
            logger.info('Group of accounts created')
    logger.info('End account creations')

    dupa1 = wallet.api.get_account(accounts[0])
    assert account_info(accounts[0], 'max_rc_creation_adjustment', wallet) == '0.000000 VESTS'
    assert account_info(accounts[0], 'delegated_rc', wallet) == 0
    assert account_info(accounts[0], 'received_delegated_rc', wallet) == 0
    wallet.api.transfer_to_vesting('initminer', accounts[0], '0.010 TESTS')
    dupa2 = wallet.api.get_account(accounts[0])
    wallet.api.transfer('initminer', accounts[0], '100.000 TESTS', '')
    dupa3 = wallet.api.get_account(accounts[0])
    wallet.api.delegate_rc(accounts[0], [accounts[1]], 10)
    dupa4 = wallet.api.get_account(accounts[0])
    assert account_info(accounts[0], 'delegated_rc', wallet) == 10
    assert account_info(accounts[1], 'received_delegated_rc', wallet) == 10
    node.wait_number_of_blocks(3)
    assert account_info(accounts[0], 'delegated_rc', wallet) == 10
    assert account_info(accounts[1], 'received_delegated_rc', wallet) == 10

    with pytest.raises(exceptions.CommunicationError):
        #Account-1 can not delegate RC to account-0 because have not enaugh Vests.
        wallet.api.delegate_rc(accounts[1], [accounts[0]], 10)

def test_same_value_rc_delegation(world):
    node = world.create_init_node()
    node.config.shared_file_size = '32G'
    node.set_clean_up_policy(constants.NodeCleanUpPolicy.DO_NOT_REMOVE_FILES)
    node.run()
    wallet = Wallet(attach_to=node)

    accounts = []
    number_of_accounts_in_one_transaction = 100
    number_of_transactions = 1
    number_of_accounts = number_of_accounts_in_one_transaction * number_of_transactions
    account_number_absolute = 0

    # created_accounts = Account.create_multiple(number_of_accounts, 'account')
    for number_of_transaction in range(number_of_transactions):
        with wallet.in_single_transaction():
            for account_number in range(number_of_accounts_in_one_transaction):
                wallet.api.create_account('initminer', f'account-{account_number_absolute}', '{}')
                accounts.append(f'account-{account_number_absolute}')
                account_number_absolute = account_number_absolute + 1
            logger.info('Group of accounts created')
    logger.info('End account creations')


    wallet.api.transfer_to_vesting('initminer', accounts[0], '0.010 TESTS')
    wallet.api.transfer('initminer', accounts[0], '100.000 TESTS', '')
    wallet.api.transfer_to_vesting('initminer', accounts[1], '0.010 TESTS')
    wallet.api.transfer('initminer', accounts[1], '100.000 TESTS', '')

    wallet.api.delegate_rc(accounts[0], [accounts[2]], 10)
    wallet.api.delegate_rc(accounts[1], [accounts[2]], 10)
    with pytest.raises(exceptions.CommunicationError):
        # Can not make same delegation RC two times
        wallet.api.delegate_rc(accounts[0], [accounts[2]], 10)

    node.wait_number_of_blocks(3)

    with pytest.raises(exceptions.CommunicationError):
        # Can not make same delegation RC two times #Dopytaj
        wallet.api.delegate_rc(accounts[0], [accounts[2]], 10)

def test_delegations_rc_to_one_receiver(world):

    node = world.create_init_node()
    node.config.shared_file_size = '32G'
    node.set_clean_up_policy(constants.NodeCleanUpPolicy.DO_NOT_REMOVE_FILES)
    node.run()
    wallet = Wallet(attach_to=node)

    accounts = []
    number_of_accounts_in_one_transaction = 120
    number_of_transactions = 1
    number_of_accounts = number_of_accounts_in_one_transaction * number_of_transactions
    account_number_absolute = 0
    delegation_account_number_absolute = 1

    # created_accounts = Account.create_multiple(number_of_accounts, 'account')
    for number_of_transaction in range(number_of_transactions):
        with wallet.in_single_transaction():
            for account_number in range(number_of_accounts_in_one_transaction):
                wallet.api.create_account('initminer', f'account-{account_number_absolute}', '{}')
                accounts.append(f'account-{account_number_absolute}')
                account_number_absolute = account_number_absolute + 1
            logger.info('Group of accounts created')
    logger.info('End account creations')

    for number_of_delegation_transaction in range(1):
        with wallet.in_single_transaction():
            for number_of_delegations in range(100):
                logger.info(f'Transfer {delegation_account_number_absolute}')
                wallet.api.transfer_to_vesting('initminer', accounts[delegation_account_number_absolute], '0.010 TESTS')
                wallet.api.transfer('initminer', accounts[delegation_account_number_absolute], '100.000 TESTS', '')
                delegation_account_number_absolute = delegation_account_number_absolute + 1

    delegation_account_number_absolute = 1
    for number_of_delegation_transaction in range(1):
        with wallet.in_single_transaction():
            for number_of_delegations in range(100):
                logger.info(f'Delegacja {number_of_delegations}')
                wallet.api.delegate_rc(accounts[delegation_account_number_absolute], [accounts[0]], 10)
                delegation_account_number_absolute = delegation_account_number_absolute + 1
    logger.info('Koniec działania')


def account_info(account, name_of_data, wallet):
    data_set = wallet.api.find_rc_accounts([account])['result'][0]
    specyfic_data = data_set[name_of_data]
    return specyfic_data

# 'delegated_rc'  'received_delegated_rc'
