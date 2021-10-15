from concurrent.futures import ThreadPoolExecutor
from test_tools import Account, logger, Wallet
import threading

def test_direct_rc_delegations(wallet, node):
    creator = 'initminer'
    delegator = 'delegator'

    logger.info('Start account creations')
    for number_of_transactions in range(9):
        with wallet.in_single_transaction():
            for accounts_in_transaction in range(1):
                wallet.api.create_account(creator, f'receiver{number_of_transactions}{accounts_in_transaction}', '{}')
    logger.info('End account creations')

    tasks_list = []
    executor = ThreadPoolExecutor(max_workers=3)
    task1 = executor.submit(delegation_rc, 'initminer', 'wallet1', node, 0, 3, 'delegations1')
    tasks_list.append(task1)

    task2 = executor.submit(delegation_rc, 'initminer', 'wallet2', node, 3, 6, 'delegations2')
    tasks_list.append(task2)

    task3 = executor.submit(delegation_rc, 'initminer', 'wallet3', node, 6, 9, 'delegations3')
    tasks_list.append(task3)

    for task in tasks_list:
        try:
            print(task.result())
        except Exception as e:
            print(e)

    # while task1.running():
    #     pass
    # return task1.result()
    #
    # while task2.running():
    #     pass
    #
    # while task3.running():
    #     pass
    # return task3.result()

    pass

def delegation_rc(creator, wallet, node, low_range, up_range, delegations):
    logger.info(f'Thread {low_range} work START')
    wallet = Wallet(attach_to=node)
    for number_of_transactions in range(low_range, up_range):
        with wallet.in_single_transaction():
            for accounts_in_transaction in range(1):
                wallet.api.delegate_rc(creator, [f'receiver{number_of_transactions}{accounts_in_transaction}'], 1)
    # delegations = wallet.api.list_rc_direct_delegations('', 1000, '')
    logger.info(f'Thread {low_range} work END')


