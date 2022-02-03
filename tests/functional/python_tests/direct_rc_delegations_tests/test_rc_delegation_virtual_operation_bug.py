from concurrent.futures import ThreadPoolExecutor

import pytest

from test_tools import Asset, constants, exceptions, logger, Wallet, World


def test_multidelegation(world: World):

    node = world.create_init_node()
    node.config.shared_file_size = '32G'
    node.run(with_arguments=('--advanced-benchmark', '--dump-memory-details', '--set-benchmark-interval', '10'),
                                environment_variables={"HIVE_HF26_TIME": "1598558400"})
    wallet = Wallet(attach_to=node)
    number_of_accounts = 10000
    logger.info('Start of account creation')
    accounts = wallet.create_accounts(number_of_accounts, 'receiver')

    # for account_number in range(len(accounts)):
    #     wallet.api.import_key(accounts[account_number].public_key)

    accounts = get_accounts_name(accounts)
    accounts_to_delegate = split_list(accounts, int(number_of_accounts / 100))
    logger.info('End of account creation')

    #przygotowanie postu
    wallet.api.create_account('initminer', 'bob', '{}')
    wallet.api.transfer_to_vesting('initminer', 'bob', Asset.Test(200))
    wallet.api.post_comment('bob', 'hello-world', '', 'xyz', 'something about world', 'just nothing', '{}')

    number_of_threads = 50
    delegators = []

    # with wallet.in_single_transaction():
    #     for thread_number in range(number_of_threads):
    #         wallet.api.create_account('initminer', f'delegator{thread_number}', '{}')
    #         delegators.append(f'delegator{thread_number}')
    #
    # with wallet.in_single_transaction():
    #     for thread_number in range(number_of_threads):
    #         wallet.api.transfer_to_vesting('initminer', f'delegator{thread_number}', Asset.Test(2000000))

    accounts_to_delegate_packs = []
    for thread_number in range(number_of_threads + 1):
        accounts_to_delegate_packs.append(int(thread_number / number_of_threads * len(accounts_to_delegate)))

    wallet.api.import_keys(accounts)
    dupa = wallet.api.list_keys()

    block_number_before_vests_transfer = int(node.get_last_block_number())
    tasks_list = []
    executor = ThreadPoolExecutor(max_workers=number_of_threads)
    for thread_number in range(number_of_threads):
        tasks_list.append(executor.submit(mass_vote, 'bob', wallet,
                                          accounts_to_delegate_packs[thread_number],
                                          accounts_to_delegate_packs[thread_number + 1],
                                          accounts_to_delegate, thread_number))

    for thread_number in tasks_list:
        try:
            thread_number.result()
        except Exception as e:
            print()
    # for thread_number in tasks_list:
    #     thread_number.result()


    block_number_after_vests_transfer = int(node.get_last_block_number())

    node.wait_number_of_blocks(22)
    api1 = node.api.account_history.enum_virtual_ops(
        block_range_begin =block_number_before_vests_transfer,
        block_range_end=block_number_after_vests_transfer,
        include_reversible=True,
        group_by_block=False,
        operation_begin=0
    )
print()

# def vests_transfer(creator, wallet: Wallet, first_accounts_pack, last_accounts_pack, accounts_to_delegate, thread_number):
#     logger.info(f'Vest transfer thread {thread_number} work START')
#     for number_of_account_pack in range(first_accounts_pack, last_accounts_pack):
#         with wallet.in_single_transaction():
#             for account in accounts_to_delegate[number_of_account_pack]:
#                 wallet.api.transfer_to_vesting(creator, account, Asset.Test(0.1))
#     logger.info(f'Vest transfer thread {thread_number} work END')

def mass_vote(post_author, wallet: Wallet, first_accounts_pack, last_accounts_pack, accounts_to_delegate, thread_number):
    logger.info(f'Vote thread {thread_number} work START')
    for number_of_account_pack in range(first_accounts_pack, last_accounts_pack):
        with wallet.in_single_transaction():
            for account in accounts_to_delegate[number_of_account_pack]:
                wallet.api.vote(account, post_author, 'hello-world', 97)
                print()
    logger.info(f'Vote thread {thread_number} work END')

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
