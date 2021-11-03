from concurrent.futures import ThreadPoolExecutor

import pytest

from test_tools import Account, Asset , constants, exceptions, logger, Wallet
import time

#TODO zamień testy  i vesty, zamień tworzenie noda,

def test_delegations_without_vests(world):
    node = world.create_init_node()
    node.run()
    wallet = Wallet(attach_to=node)

    accounts = []
    number_of_accounts_in_one_transaction = 100
    number_of_transactions = 1
    account_number_absolute = 0

    for number_of_transaction in range(number_of_transactions):
        with wallet.in_single_transaction():
            for account_number in range(number_of_accounts_in_one_transaction):
                wallet.api.create_account('initminer', f'account-{account_number_absolute}', '{}')
                accounts.append(f'account-{account_number_absolute}')
                account_number_absolute = account_number_absolute + 1
            logger.info('Group of accounts created')
    logger.info('End account creations')

    assert rc_account_info(accounts[0], 'max_rc_creation_adjustment', wallet) == Asset.Vest(0)
    assert rc_account_info(accounts[0], 'delegated_rc', wallet) == 0
    assert rc_account_info(accounts[0], 'received_delegated_rc', wallet) == 0
    assert account_info(accounts[0], 'vesting_shares', wallet) == Asset.Vest(0)

    wallet.api.transfer_to_vesting('initminer', accounts[0], Asset.Test(100))

    assert account_info(accounts[0], 'vesting_shares', wallet) != Asset.Vest(0)

    wallet.api.transfer('initminer', accounts[0], Asset.Test(100000), '')
    wallet.api.delegate_rc(accounts[0], [accounts[1]], 10)

    assert rc_account_info(accounts[0], 'delegated_rc', wallet) == 10
    assert rc_account_info(accounts[1], 'received_delegated_rc', wallet) == 10

    node.wait_number_of_blocks(3)

    assert rc_account_info(accounts[0], 'delegated_rc', wallet) == 10
    assert rc_account_info(accounts[1], 'received_delegated_rc', wallet) == 10

    with pytest.raises(exceptions.CommunicationError):
        # Account-1 can not delegate RC to account-0 because have not enaugh Vests.
        wallet.api.delegate_rc(accounts[1], [accounts[0]], 10)


def test_same_value_rc_delegation(world):
    node = world.create_init_node()
    node.run()
    wallet = Wallet(attach_to=node)

    accounts = []
    number_of_accounts_in_one_transaction = 100
    number_of_transactions = 1
    account_number_absolute = 0

    for number_of_transaction in range(number_of_transactions):
        with wallet.in_single_transaction():
            for account_number in range(number_of_accounts_in_one_transaction):
                wallet.api.create_account('initminer', f'account-{account_number_absolute}', '{}')
                accounts.append(f'account-{account_number_absolute}')
                account_number_absolute = account_number_absolute + 1
            logger.info('Group of accounts created')
    logger.info('End account creations')

    wallet.api.transfer_to_vesting('initminer', accounts[0], Asset.Test(10))
    wallet.api.transfer('initminer', accounts[0], Asset.Test(100000), '')
    wallet.api.transfer_to_vesting('initminer', accounts[1], Asset.Test(10))
    wallet.api.transfer('initminer', accounts[1], Asset.Test(100000), '')

    wallet.api.delegate_rc(accounts[0], [accounts[2]], 10)
    wallet.api.delegate_rc(accounts[1], [accounts[2]], 10)

    with pytest.raises(exceptions.CommunicationError):
        # Can not make same delegation RC two times
        wallet.api.delegate_rc(accounts[0], [accounts[2]], 10)

    node.wait_number_of_blocks(3)

    with pytest.raises(exceptions.CommunicationError):
        # Can not make same delegation RC two times
        wallet.api.delegate_rc(accounts[0], [accounts[2]], 10)


def test_delegations_rc_to_one_receiver(world):
    node = world.create_init_node()
    node.run()
    wallet = Wallet(attach_to=node)

    accounts = []
    number_of_accounts_in_one_transaction = 120
    number_of_transactions = 1
    account_number_absolute = 0
    delegation_account_number_absolute = 1

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
                wallet.api.transfer_to_vesting('initminer', accounts[delegation_account_number_absolute], Asset.Test(100000))
                wallet.api.transfer('initminer', accounts[delegation_account_number_absolute], Asset.Test(10), '')
                delegation_account_number_absolute = delegation_account_number_absolute + 1

    delegation_account_number_absolute = 1
    for number_of_delegation_transaction in range(1):
        with wallet.in_single_transaction():
            for number_of_delegations in range(100):
                logger.info(f'Delegacja {number_of_delegations}')
                wallet.api.delegate_rc(accounts[delegation_account_number_absolute], [accounts[0]], 10)
                delegation_account_number_absolute = delegation_account_number_absolute + 1


def test_recall_rc_delegation_in_chain_how_work(world):
    node = world.create_init_node()
    node.run()
    wallet = Wallet(attach_to=node)

    accounts = []
    number_of_accounts_in_one_transaction = 100
    number_of_transactions = 1
    account_number_absolute = 0

    for number_of_transaction in range(number_of_transactions):
        with wallet.in_single_transaction():
            for account_number in range(number_of_accounts_in_one_transaction):
                wallet.api.create_account('initminer', f'account-{account_number_absolute}', '{}')
                accounts.append(f'account-{account_number_absolute}')
                account_number_absolute = account_number_absolute + 1
            logger.info('Group of accounts created')
    logger.info('End account creations')

    wallet.api.transfer_to_vesting('initminer', accounts[0], Asset.Test(0.01))
    wallet.api.transfer_to_vesting('initminer', accounts[1], Asset.Test(0.01))
    wallet.api.transfer_to_vesting('initminer', accounts[2], Asset.Test(0.01))
    wallet.api.transfer_to_vesting('initminer', accounts[3], Asset.Test(0.01))

    assert rc_account_info(accounts[0], 'rc_manabar', wallet)['current_mana'] == 1664
    assert rc_account_info(accounts[1], 'rc_manabar', wallet)['current_mana'] == 1373
    assert rc_account_info(accounts[2], 'rc_manabar', wallet)['current_mana'] == 1185
    assert rc_account_info(accounts[3], 'rc_manabar', wallet)['current_mana'] == 1052

    assert rc_account_info(accounts[0], 'max_rc', wallet) == 1664
    assert rc_account_info(accounts[1], 'max_rc', wallet) == 1373
    assert rc_account_info(accounts[2], 'max_rc', wallet) == 1185
    assert rc_account_info(accounts[3], 'max_rc', wallet) == 1052

    wallet.api.delegate_rc(accounts[0], [accounts[1]], 10)
    wallet.api.delegate_rc(accounts[1], [accounts[2]], 10)
    wallet.api.delegate_rc(accounts[2], [accounts[3]], 10)

    assert rc_account_info(accounts[0], 'rc_manabar', wallet)['current_mana'] == 1651
    assert rc_account_info(accounts[1], 'rc_manabar', wallet)['current_mana'] == 1370
    assert rc_account_info(accounts[2], 'rc_manabar', wallet)['current_mana'] == 1182
    assert rc_account_info(accounts[3], 'rc_manabar', wallet)['current_mana'] == 1062

    assert rc_account_info(accounts[0], 'max_rc', wallet) == 1654
    assert rc_account_info(accounts[1], 'max_rc', wallet) == 1373
    assert rc_account_info(accounts[2], 'max_rc', wallet) == 1185
    assert rc_account_info(accounts[3], 'max_rc', wallet) == 1062

    wallet.api.delegate_rc(accounts[0], [accounts[1]], 0)

    assert rc_account_info(accounts[0], 'rc_manabar', wallet)['current_mana'] == 1648
    assert rc_account_info(accounts[1], 'rc_manabar', wallet)['current_mana'] == 1360
    assert rc_account_info(accounts[2], 'rc_manabar', wallet)['current_mana'] == 1182
    assert rc_account_info(accounts[3], 'rc_manabar', wallet)['current_mana'] == 1062

    assert rc_account_info(accounts[0], 'max_rc', wallet) == 1664
    assert rc_account_info(accounts[1], 'max_rc', wallet) == 1363
    assert rc_account_info(accounts[2], 'max_rc', wallet) == 1185
    assert rc_account_info(accounts[3], 'max_rc', wallet) == 1062

    node.wait_number_of_blocks(4)

    assert rc_account_info(accounts[0], 'rc_manabar', wallet)['current_mana'] == 1648
    assert rc_account_info(accounts[1], 'rc_manabar', wallet)['current_mana'] == 1360
    assert rc_account_info(accounts[2], 'rc_manabar', wallet)['current_mana'] == 1182
    assert rc_account_info(accounts[3], 'rc_manabar', wallet)['current_mana'] == 1062

    assert rc_account_info(accounts[0], 'max_rc', wallet) == 1664
    assert rc_account_info(accounts[1], 'max_rc', wallet) == 1363
    assert rc_account_info(accounts[2], 'max_rc', wallet) == 1185
    assert rc_account_info(accounts[3], 'max_rc', wallet) == 1062

    wallet.api.delegate_rc(accounts[1], [accounts[2]], 20)
    wallet.api.delegate_rc(accounts[3], [accounts[2]], 15)

    assert rc_account_info(accounts[0], 'rc_manabar', wallet)['current_mana'] == 1648
    assert rc_account_info(accounts[1], 'rc_manabar', wallet)['current_mana'] == 1347
    assert rc_account_info(accounts[2], 'rc_manabar', wallet)['current_mana'] == 1207
    assert rc_account_info(accounts[3], 'rc_manabar', wallet)['current_mana'] == 1044

    assert rc_account_info(accounts[0], 'max_rc', wallet) == 1664
    assert rc_account_info(accounts[1], 'max_rc', wallet) == 1353
    assert rc_account_info(accounts[2], 'max_rc', wallet) == 1210
    assert rc_account_info(accounts[3], 'max_rc', wallet) == 1047

def test_delegation_delegated_rc(world):
    node = world.create_init_node()
    node.run()
    wallet = Wallet(attach_to=node)

    accounts = []
    number_of_accounts_in_one_transaction = 400
    number_of_transactions = 1
    account_number_absolute = 0
    delegation_account_number_absolute = 0

    for number_of_transaction in range(number_of_transactions):
        with wallet.in_single_transaction():
            for account_number in range(number_of_accounts_in_one_transaction):
                wallet.api.create_account('initminer', f'account-{account_number_absolute}', '{}')
                accounts.append(f'account-{account_number_absolute}')
                account_number_absolute = account_number_absolute + 1
            logger.info('Group of accounts created')
    logger.info('End account creations')

    wallet.api.transfer_to_vesting('initminer', accounts[0], Asset.Test(0.1))
    wallet.api.transfer('initminer', accounts[0], Asset.Test(1), '')
    wallet.api.transfer_to_vesting('initminer', accounts[1], Asset.Test(0.001))
    wallet.api.transfer('initminer', accounts[1], Asset.Test(0.01), '')
    number_of_rc = rc_account_info(accounts[0], 'rc_manabar', wallet)['current_mana']

    MANA0 = rc_account_info(accounts[0], 'rc_manabar', wallet)['current_mana']
    wallet.api.delegate_rc(accounts[0], [accounts[1]], number_of_rc - 3)
    MANA1 = rc_account_info(accounts[0], 'rc_manabar', wallet)['current_mana']
    MANA2 = rc_account_info(accounts[1], 'rc_manabar', wallet)['current_mana']
    MANA5 = rc_account_info(accounts[1], 'max_rc', wallet)
    wallet.api.delegate_rc(accounts[1], [accounts[2]], number_of_rc - 3 - 3)
    MANA3 = rc_account_info(accounts[1], 'rc_manabar', wallet)['current_mana']
    MANA4 = rc_account_info(accounts[2], 'rc_manabar', wallet)['current_mana']
    wallet.api.delegate_rc(accounts[2], [accounts[3]], number_of_rc - 3 - 3 - 3)
    wallet.api.delegate_rc(accounts[3], [accounts[4]], number_of_rc - 3 - 3 - 3 - 3)

    pass






def rc_account_info(account, name_of_data, wallet):
    data_set = wallet.api.find_rc_accounts([account])['result'][0]
    specyfic_data = data_set[name_of_data]
    return specyfic_data


def account_info(account, name_of_data, wallet):
    data_set = wallet.api.get_account(account)['result']
    specyfic_data = data_set[name_of_data]
    return specyfic_data

# 'delegated_rc'  'received_delegated_rc' 'rc_manabar'
