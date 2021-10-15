from concurrent.futures import ThreadPoolExecutor
from test_tools import logger, Wallet


def test_direct_rc_delegations(wallet, node):
    creator = 'initminer'
    delegator = 'delegator'
    client_accounts = []
    num = 0
    logger.info('Start account creations')
    for number_of_transactions in range(1100):
        if number_of_transactions % 10 == 0:
            print()

        with wallet.in_single_transaction():
            for accounts_in_transaction in range(200):
                wallet.api.create_account(creator, f'receiver{num}', '{}')
                client_accounts.append(f'receiver{num}')
                num = num + 1
        logger.info('Group of accounts created')
    logger.info('End account creations')
    accounts_to_delegate = split_list(client_accounts, 12500)
    logger.info('End of splitting')

    tasks_list = []
    executor = ThreadPoolExecutor(max_workers=10)

    task0= executor.submit(delegation_rc, 'initminer', 'wallet1', node, 0, 3125, accounts_to_delegate)
    tasks_list.append(task0)

    task1 = executor.submit(delegation_rc, 'initminer', 'wallet1', node, 3125, 6250, accounts_to_delegate)
    tasks_list.append(task1)

    task2 = executor.submit(delegation_rc, 'initminer', 'wallet2', node, 6250, 9375, accounts_to_delegate)
    tasks_list.append(task2)

    task3 = executor.submit(delegation_rc, 'initminer', 'wallet3', node, 9375, 12500, accounts_to_delegate)
    tasks_list.append(task3)

    for task in tasks_list:
        try:
            print(task.result())
        except Exception as e:
            print(e)

    pass


def delegation_rc(creator, wallet, node, first_accounts_pack, last_accounts_pack, accounts_to_delegate):
    logger.info(f'Thread {first_accounts_pack} work START')
    wallet = Wallet(attach_to=node)
    for number_of_account_pack in range(first_accounts_pack, last_accounts_pack):
        wallet.api.delegate_rc(creator, accounts_to_delegate[number_of_account_pack], 1)
    # delegations = wallet.api.list_rc_direct_delegations('', 1000, '')
    logger.info(f'Thread {first_accounts_pack} work END')


def split_list(alist, wanted_parts):
    length = len(alist)
    return [alist[i * length // wanted_parts: (i + 1) * length // wanted_parts]
            for i in range(wanted_parts)]
