from concurrent.futures import ThreadPoolExecutor
from test_tools import Account, Asset,  constants, logger, Wallet
import time


def test_direct_rc_delegations(world):
    node = world.create_init_node()
    # node.config.shared_file_size = '32G'
    # node.set_clean_up_policy(constants.NodeCleanUpPolicy.DO_NOT_REMOVE_FILES)
    node.run()
    wallet = Wallet(attach_to=node)

    #przygotowanie postu
    wallet.api.create_account('initminer', 'bob', '{}')
    wallet.api.transfer_to_vesting('initminer', 'bob', Asset.Test(20000000))
    wallet.api.post_comment('bob', 'hello-world', '', 'xyz', 'something about world', 'just nothing', '{}')

    client_accounts = []
    number_of_accounts_in_one_transaction = 110
    number_of_transactions = 10
    number_of_accounts = number_of_accounts_in_one_transaction * number_of_transactions
    account_number_absolute = 0
    wallets = []

    accounts = Account.create_multiple(number_of_accounts, 'receiver')
    for number_of_transaction in range(number_of_transactions):
        wallet = Wallet(attach_to=node)
        wallets.append(wallet)
        with wallet.in_single_transaction():
            for account_number in range(number_of_accounts_in_one_transaction):
                account_name = accounts[account_number_absolute].name
                account_key = accounts[account_number_absolute].public_key
                wallet.api.create_account_with_keys('initminer', account_name, '{}', account_key, account_key,
                                                    account_key, account_key)
                client_accounts.append(account_name)
                account_number_absolute = account_number_absolute + 1
            logger.info('Group of accounts created')
    logger.info('End account creations')
    accounts_to_delegate = split_list(client_accounts, int(number_of_accounts / 100))
    logger.info('End of splitting')

    # number_of_threads = 10
    # delegators = []
    # for thread_number in range(number_of_threads):
    # 
    #     wallet.api.create_account('initminer', f'delegator{thread_number}', '{}')
    #     delegators.append(f'delegator{thread_number}')
    #     wallet.api.transfer_to_vesting('initminer', f'delegator{thread_number}', '100.010 TESTS')

    # accounts_to_delegate_packs = []
    # for thread_number in range(number_of_threads + 1):
    #     accounts_to_delegate_packs.append(int(thread_number / number_of_threads * len(accounts_to_delegate)))


    # tasks_list = []
    # executor = ThreadPoolExecutor(max_workers=number_of_threads)
    # for thread_number in range(number_of_threads-1):
    #     tasks_list.append(executor.submit(delegation_rc, wallets[thread_number],
    #                                       accounts_to_delegate[thread_number], thread_number))

    # for thread_number in tasks_list:
    #     thread_number.result()

    node.wait_number_of_blocks(20)
    tasks_list = []
    executor = ThreadPoolExecutor(max_workers=10)
    task0 = executor.submit(delegation_rc, 'InitNode.Wallet0', 'initminer', 0)
    tasks_list.append(task0)

    task1 = executor.submit(delegation_rc, 'InitNode.Wallet1',  'initminer', 1)
    tasks_list.append(task1)

    task2 = executor.submit(delegation_rc, 'InitNode.Wallet2', 'initminer', 2)
    tasks_list.append(task2)

    task3 = executor.submit(delegation_rc, 'InitNode.Wallet3', accounts_to_delegate[3], 3)
    tasks_list.append(task3)

    task4 = executor.submit(delegation_rc, 'InitNode.Wallet4',  accounts_to_delegate[4], 4)
    tasks_list.append(task4)

    task5 = executor.submit(delegation_rc, 'InitNode.Wallet5', accounts_to_delegate[5], 5)
    tasks_list.append(task5)

    task6 = executor.submit(delegation_rc, 'InitNode.Wallet6', accounts_to_delegate[6], 6)
    tasks_list.append(task6)

    task7 = executor.submit(delegation_rc, 'InitNode.Wallet7',  accounts_to_delegate[7], 7)
    tasks_list.append(task7)

    task8 = executor.submit(delegation_rc, 'InitNode.Wallet8', accounts_to_delegate[8], 8)
    tasks_list.append(task8)

    task9 = executor.submit(delegation_rc, 'InitNode.Wallet9', accounts_to_delegate[9], 9)
    tasks_list.append(task9)


    node.wait_number_of_blocks(50)
    api1 = node.api.account_history.enum_virtual_ops(
        block_range_begin=20,
        block_range_end=50,
        include_reversible=False,
        group_by_block=False,
        operation_begin=0
    )

    logger.info(api1)
    time.sleep(10000)

def delegation_rc(wallet, accounts_to_delegate, thread_number):
    logger.info(f'Thread {thread_number} work START')
    with wallet.in_single_transaction():
                for x in range(99):
                    wallet.api.vote(accounts_to_delegate[x], 'bob', 'hello-world', 98)
    logger.info(f'Thread {thread_number} work END')


def split_list(alist, wanted_parts):
    length = len(alist)
    return [alist[i * length // wanted_parts: (i + 1) * length // wanted_parts]
            for i in range(wanted_parts)]
