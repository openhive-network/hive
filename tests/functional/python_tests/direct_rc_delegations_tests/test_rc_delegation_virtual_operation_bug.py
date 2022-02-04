import time
from concurrent.futures import ThreadPoolExecutor

import pytest

from test_tools import Asset, constants, exceptions, logger, Wallet, World


def test_multidelegation(world: World):
    network = world.create_network()
    init_node = network.create_init_node()
    api_node = network.create_api_node()
    init_node.config.shared_file_size = '32G'
    network.run()
    wallet_initnode = Wallet(attach_to=init_node)
    wallet_apinode = Wallet(attach_to=api_node)

    number_of_accounts = 10000
    logger.info('Start of account creation')
    accounts = wallet_initnode.create_accounts(number_of_accounts, 'receiver')
    accounts = get_accounts_name(accounts)
    accounts_to_delegate = split_list(accounts, int(number_of_accounts / 5))
    logger.info('End of account creation')

    #przygotowanie postu
    wallet_initnode.api.create_account('initminer', 'bob', '{}')
    wallet_initnode.api.transfer_to_vesting('initminer', 'bob', Asset.Test(200))
    wallet_initnode.api.post_comment('bob', 'hello-world', '', 'xyz', 'something about world', 'just nothing', '{}')
    wallet_initnode.api.vote('initminer','bob', 'hello-world', 97)
    number_of_threads = 1000
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


    logger.info(f'Api node block {api_node.get_last_block_number()}')
    logger.info(f'Init node block {init_node.get_last_block_number()}')

    block_number_before_vests_transfer = int(api_node.get_last_block_number())
    tasks_list = []
    executor = ThreadPoolExecutor(max_workers=number_of_threads)
    for thread_number in range(number_of_threads):
        tasks_list.append(executor.submit(mass_vote, 'bob', wallet_apinode,
                                          accounts_to_delegate_packs[thread_number],
                                          accounts_to_delegate_packs[thread_number + 1],
                                          accounts_to_delegate, thread_number, executor))

    for thread_number in tasks_list:
        try:
            thread_number.result()
        except Exception as e:
            print()
    # for thread_number in tasks_list:
    #     thread_number.result()

    logger.info('Zaraz startuje testowanie eunuma')
    block_number_after_vests_transfer = int(api_node.get_last_block_number())

    api_node.wait_number_of_blocks(22)
    api1 = api_node.api.account_history.enum_virtual_ops(
        block_range_begin =block_number_before_vests_transfer-1,
        block_range_end=block_number_after_vests_transfer+1,
        include_reversible=True,
        group_by_block=False,
        operation_begin=0
    )
    print()
    time.sleep(213255)
# def vests_transfer(creator, wallet: Wallet, first_accounts_pack, last_accounts_pack, accounts_to_delegate, thread_number):
#     logger.info(f'Vest transfer thread {thread_number} work START')
#     for number_of_account_pack in range(first_accounts_pack, last_accounts_pack):
#         with wallet.in_single_transaction():
#             for account in accounts_to_delegate[number_of_account_pack]:
#                 wallet.api.transfer_to_vesting(creator, account, Asset.Test(0.1))
#     logger.info(f'Vest transfer thread {thread_number} work END')

def mass_vote(post_author, wallet: Wallet, first_accounts_pack, last_accounts_pack, accounts_to_delegate, thread_number,executor:ThreadPoolExecutor):
    logger.info(f'Vote thread {thread_number} work START')
    for number_of_account_pack in range(first_accounts_pack, last_accounts_pack):
        for account in accounts_to_delegate[number_of_account_pack]:
            logger.info(f'Vote thread {thread_number}, voting {account} now, last {(accounts_to_delegate[last_accounts_pack])[-1]}')
            try:
                wallet.api.vote(account, post_author, 'hello-world', 97)
            except:
                pass
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
