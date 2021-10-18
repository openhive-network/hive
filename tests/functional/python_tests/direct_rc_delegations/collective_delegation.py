from concurrent.futures import ThreadPoolExecutor
from test_tools import context, logger, Wallet
import time


def test_direct_rc_delegations(world):
    node = world.create_init_node()
    node.run(load_snapshot_from='/home/dev/src/hive/tests/functional/python_tests/direct_rc_delegations/generated_during_creation_accounts_snapshot/test_direct_rc_delegations/InitNode/snapshot')

    client_accounts = []
    number_of_accounts_in_one_transaction = 400
    number_of_transactions = 2500
    number_of_accounts = number_of_accounts_in_one_transaction * number_of_transactions

    for account_number_absolute in range(number_of_accounts):
        client_accounts.append(f'receiver-{account_number_absolute}')
    accounts_to_delegate = split_list(client_accounts, int(number_of_accounts / 100))
    logger.info('End of splitting')

    number_of_threads = 10
    wallets = []
    delegators = []

    for thread_number in range(number_of_threads):
        wallet = Wallet(attach_to=node)
        wallets.append(wallet)
        wallet.api.create_account('initminer', f'delegator{thread_number}', '{}')
        wallet.api.set_password('password')
        wallet.api.unlock('password')
        delegators.append(f'delegator{thread_number}')
        wallet.api.transfer_to_vesting('initminer', f'delegator{thread_number}', '100.010 TESTS')

    accounts_to_delegate_packs = []
    for thread_number in range(number_of_threads + 1):
        accounts_to_delegate_packs.append(int(thread_number / number_of_threads * len(accounts_to_delegate)))

    tasks_list = []
    executor = ThreadPoolExecutor(max_workers=number_of_threads)
    for thread_number in range(number_of_threads):
        tasks_list.append(executor.submit(delegation_rc, delegators[thread_number], wallets[thread_number],
                                          accounts_to_delegate_packs[thread_number],
                                          accounts_to_delegate_packs[thread_number + 1],
                                          accounts_to_delegate, thread_number))

    for thread_number in tasks_list:
        thread_number.result()


def delegation_rc(creator, wallet, first_accounts_pack, last_accounts_pack, accounts_to_delegate, thread_number):
    logger.info(f'Thread {thread_number} work START')
    for number_of_account_pack in range(first_accounts_pack, last_accounts_pack):
        wallet.api.delegate_rc(creator, accounts_to_delegate[number_of_account_pack], 1)
    logger.info(f'Thread {thread_number} work END')


def split_list(alist, wanted_parts):
    length = len(alist)
    return [alist[i * length // wanted_parts: (i + 1) * length // wanted_parts]
            for i in range(wanted_parts)]
