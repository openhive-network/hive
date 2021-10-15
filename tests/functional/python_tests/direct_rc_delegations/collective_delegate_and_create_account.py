from concurrent.futures import ThreadPoolExecutor
from test_tools import Account, logger, Wallet
import time

def test_direct_rc_delegations(wallet, node):
    creator = 'initminer'
    delegator = 'delegator'
    client_accounts = []
    number_of_accounts = 4000
    number_of_accounts_in_one_transaction = 200
    number_of_transactions = int(number_of_accounts / number_of_accounts_in_one_transaction)
    account_number_absolute = 0

    accounts = Account.create_multiple(number_of_accounts, 'receiver')

    for number_of_transaction in range(number_of_transactions):
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
    accounts_to_delegate = split_list(client_accounts, int(number_of_accounts/100))
    logger.info('End of splitting')

    number_of_threads = 4
    tasks_list = []
    executor = ThreadPoolExecutor(max_workers=number_of_threads)


    for thread_number in number_of_threads:

        delegator_name = accounts[thread_number].name
        delegator_key = accounts[thread_number].public_key
        wallet.api.create_account_with_keys('initminer', delegator_key, '{}', delegator_key, delegator_key,
                                            delegator_key, delegator_key)

        thread_number = executor.submit(delegator_name, f'wallet{thread_number}', node, int(thread_number/number_of_threads * number_of_accounts))
    task0 = executor.submit(delegation_rc, 'initminer', 'wallet1', node, 0, int(1/4 * number_of_accounts), accounts_to_delegate)
    tasks_list.append(task0)

    task1 = executor.submit(delegation_rc, 'initminer', 'wallet1', node, int(1/4 * number_of_accounts), int(2/4 * number_of_accounts), accounts_to_delegate)
    tasks_list.append(task1)

    task2 = executor.submit(delegation_rc, 'initminer', 'wallet2', node, int(2/4 * number_of_accounts), int(3/4 * number_of_accounts), accounts_to_delegate)
    tasks_list.append(task2)

    task3 = executor.submit(delegation_rc, 'initminer', 'wallet3', node, int(3/4 * number_of_accounts), number_of_accounts, accounts_to_delegate)
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
    logger.info(f'Thread {first_accounts_pack} work END')


def split_list(alist, wanted_parts):
    length = len(alist)
    return [alist[i * length // wanted_parts: (i + 1) * length // wanted_parts]
            for i in range(wanted_parts)]

