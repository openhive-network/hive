import time
from concurrent.futures import ThreadPoolExecutor
from test_tools import Account, logger, Wallet
import threading

def test_direct_rc_delegations(wallet, node):
    # logger.info('Start account creations')
    # for number_of_transactions in range(9):
    #     with wallet.in_single_transaction():
    #         for accounts_in_transaction in range(1):
    #             wallet.api.create_account(creator, f'receiver{number_of_transactions}{accounts_in_transaction}', '{}')
    # logger.info('End account creations')

    wallet1 = Wallet(attach_to=node)
    wallet2 = Wallet(attach_to=node)
    wallet3 = Wallet(attach_to=node)

    accounts_thread_1 = []
    accounts_thread_2 = []
    accounts_thread_3 = []

    tasks_list = []
    executor = ThreadPoolExecutor(max_workers=3)
    task1 = executor.submit(delegation_rc, wallet1, 1, accounts_thread_1)
    tasks_list.append(task1)

    task2 = executor.submit(delegation_rc, wallet2, 2, accounts_thread_2)
    tasks_list.append(task2)

    task3 = executor.submit(delegation_rc, wallet3, 3, accounts_thread_3)
    tasks_list.append(task3)

    for task in tasks_list:
        try:
            print(task.result())
        except Exception as e:
            print(e)

    dupa1 = accounts_thread_1
    dupa2 = accounts_thread_2
    dupa3 = accounts_thread_3

    api1 = node.api.account_history.get_account_history(
        account='initminer',
        start=-1,
        limit=1000
    )

    api2 = node.api.account_history.enum_virtual_ops(
        block_range_begin=13,
        block_range_end=40,
        include_reversible=True,
        group_by_block=False,
        operation_begin=0
    )

    api3 = node.api.account_history.get_ops_in_block(
        block_num=15,
        only_virtual=False,
        include_reversible=True
    )


    wallet.api.transfer_to_vesting('initminer', 'receiver2-1', '100.010 TESTS')
    wallet.api.delegate_rc('receiver2-1', ['receiver2-2'], 1)

    pass

def delegation_rc(wallet, thread_number, client_accounts):
    logger.info(f'Thread {thread_number} work START')
    logger.info(f'Start account creations---THREAD{thread_number}')
    number_of_accounts_in_one_transaction = 50
    number_of_transactions = 20
    number_of_accounts = number_of_accounts_in_one_transaction * number_of_transactions
    account_number_absolute = 0
    accounts = Account.create_multiple(number_of_accounts, f'receiver{thread_number}')
    for number_of_transaction in range(number_of_transactions):
        with wallet.in_single_transaction():
            for account_number in range(number_of_accounts_in_one_transaction):
                account_name = accounts[account_number_absolute].name
                account_key = accounts[account_number_absolute].public_key
                wallet.api.create_account_with_keys('initminer', account_name, '{}', account_key, account_key,
                                                    account_key, account_key)
                client_accounts.append(account_name)
                account_number_absolute = account_number_absolute + 1
    logger.info(f'End account creations----THREAD{thread_number}')
    logger.info(f'Thread {thread_number} work END')
