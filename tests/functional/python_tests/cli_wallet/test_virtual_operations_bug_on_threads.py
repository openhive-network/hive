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
    number_of_accounts_in_one_transaction = 400
    number_of_transactions = 10
    number_of_accounts = number_of_accounts_in_one_transaction * number_of_transactions
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
    accounts_to_delegate = split_list(client_accounts, int(number_of_accounts / 100))
    logger.info('End of splitting')

    number_of_threads = 10
    wallets = []
    delegators = []
    # for thread_number in range(number_of_threads):
    #     wallet = Wallet(attach_to=node)
    #     wallets.append(wallet)
    #     wallet.api.create_account('initminer', f'delegator{thread_number}', '{}')
    #     delegators.append(f'delegator{thread_number}')
    #     wallet.api.transfer_to_vesting('initminer', f'delegator{thread_number}', '100.010 TESTS')

    # accounts_to_delegate_packs = []
    # for thread_number in range(number_of_threads + 1):
    #     accounts_to_delegate_packs.append(int(thread_number / number_of_threads * len(accounts_to_delegate)))

    tasks_list = []
    executor = ThreadPoolExecutor(max_workers=number_of_threads)
    for thread_number in range(number_of_threads-1):
        tasks_list.append(executor.submit(delegation_rc, wallets[thread_number],
                                          accounts_to_delegate[thread_number], thread_number))

    for thread_number in tasks_list:
        thread_number.result()


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
