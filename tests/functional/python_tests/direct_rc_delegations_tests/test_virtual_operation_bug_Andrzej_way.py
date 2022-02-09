import os
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

    number_of_accounts = 100
    logger.info('Start of account creation')
    accounts = wallet_initnode.create_accounts(number_of_accounts, 'receiver')
    accounts = get_accounts_name(accounts)
    accounts_to_delegate = split_list(accounts, int(number_of_accounts / 5))
    logger.info('End of account creation')

    #przygotowanie postu
    wallet_initnode.api.create_account('initminer', 'bob', '{}')
    wallet_initnode.api.transfer_to_vesting('initminer', 'bob', Asset.Test(200))
    wallet_initnode.api.post_comment('bob', 'hello-world', '', 'xyz', 'something about world', 'just nothing', '{}')
    wallet_initnode.api.vote('initminer', 'bob', 'hello-world', 97)

    with wallet_initnode.in_single_transaction():
        for account in accounts:
            wallet_initnode.api.transfer_to_vesting('initminer', account, Asset.Test(2000000))

    logger.info(f'Api node block {api_node.get_last_block_number()}')
    logger.info(f'Init node block {init_node.get_last_block_number()}')

    with wallet_initnode.in_single_transaction():
        wallet_initnode.api.post_comment('receiver-0', 'hello-world0', '', 'xyz', 'something about world', 'just nothing', '{}')
        wallet_initnode.api.post_comment('receiver-1', 'hello-world1', '', 'xyz', 'something about world', 'just nothing', '{}')
        wallet_initnode.api.post_comment('receiver-2', 'hello-world2', '', 'xyz', 'something about world', 'just nothing', '{}')
        wallet_initnode.api.post_comment('receiver-3', 'hello-world3', '', 'xyz', 'something about world', 'just nothing', '{}')
        wallet_initnode.api.post_comment('receiver-4', 'hello-world4', '', 'xyz', 'something about world', 'just nothing', '{}')
        wallet_initnode.api.post_comment('receiver-5', 'hello-world5', '', 'xyz', 'something about world', 'just nothing', '{}')
        wallet_initnode.api.post_comment('receiver-6', 'hello-world6', '', 'xyz', 'something about world', 'just nothing', '{}')
        wallet_initnode.api.post_comment('receiver-7', 'hello-world7', '', 'xyz', 'something about world', 'just nothing', '{}')
        wallet_initnode.api.post_comment('receiver-8', 'hello-world8', '', 'xyz', 'something about world', 'just nothing', '{}')
        wallet_initnode.api.post_comment('receiver-9', 'hello-world9', '', 'xyz', 'something about world', 'just nothing', '{}')

    tasks_list = []
    executor = ThreadPoolExecutor(max_workers=2)
    tasks_list.append(executor.submit(enum_testing, api_node, wallet_apinode))
    tasks_list.append(executor.submit(voting, wallet_initnode, init_node))

    for thread_number in tasks_list:
        try:
            thread_number.result()
        except Exception as e:
            print()


def voting(wallet_initnode, init_node):
    logger.info(f'Ostatni wyprodukowany blok przed votem z "receiver-0" to :{init_node.get_last_block_number()}')
    wallet_initnode.api.vote('receiver-20', 'receiver-0', 'hello-world0', 97)

    logger.info(f'Ostatni wyprodukowany blok przed votem z "receiver-1" to :{init_node.get_last_block_number()}')
    wallet_initnode.api.vote('receiver-21', 'receiver-1', 'hello-world1', 97)

    logger.info(f'Ostatni wyprodukowany blok przed votem z "receiver-2" to :{init_node.get_last_block_number()}')
    wallet_initnode.api.vote('receiver-22', 'receiver-2', 'hello-world2', 97)

    logger.info(f'Ostatni wyprodukowany blok przed votem z "receiver-3 to :"{init_node.get_last_block_number()}')
    wallet_initnode.api.vote('receiver-23', 'receiver-3', 'hello-world3', 97)

    logger.info(f'Ostatni wyprodukowany blok przed votem z "receiver-4 to:"{init_node.get_last_block_number()}')
    wallet_initnode.api.vote('receiver-24', 'receiver-4', 'hello-world4', 97)

    logger.info(f'Ostatni wyprodukowany blok przed votem z "receiver-5" to:{init_node.get_last_block_number()}')
    wallet_initnode.api.vote('receiver-25', 'receiver-5', 'hello-world5', 97)

    logger.info(f'Ostatni wyprodukowany blok przed votem z "receiver-6" to:{init_node.get_last_block_number()}')
    wallet_initnode.api.vote('receiver-26', 'receiver-6', 'hello-world6', 97)

    logger.info(f'Ostatni wyprodukowany blok przed votem z "receiver-7" to:{init_node.get_last_block_number()}')
    wallet_initnode.api.vote('receiver-27', 'receiver-7', 'hello-world7', 97)

    logger.info(f'Ostatni wyprodukowany blok przed votem z "receiver-8 to:"{init_node.get_last_block_number()}')
    wallet_initnode.api.vote('receiver-28', 'receiver-8', 'hello-world8', 97)

    logger.info(f'Ostatni wyprodukowany blok przed votem z "receiver-9 to:"{init_node.get_last_block_number()}')
    wallet_initnode.api.vote('receiver-29', 'receiver-9', 'hello-world9', 97)

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
        for account in accounts_to_delegate[number_of_account_pack]:
            logger.info(f'Vote thread {thread_number}, voting {account} now, last {(accounts_to_delegate[last_accounts_pack])[-1]}')
            try:
                wallet.api.vote(account, post_author, 'hello-world', 97)
            except:
                pass
    logger.info(f'Vote thread {thread_number} work END')

def enum_testing(api_node, wallet_apinode):
    if os.path.exists(wallet_apinode.directory / 'enum_virtual_op_file.txt'):
        os.remove(wallet_apinode.directory / 'enum_virtual_op_file.txt')
    logger.info('ENUM TESTING RUSZA!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!')
    block_num = api_node.get_last_block_number
    while True:
        vote_virtual_ops = []
        file = open(wallet_apinode.directory / 'enum_virtual_op_file.txt', "a")
        block_number = api_node.get_last_block_number()
        file.write(f'Ostatni wyprodukowany blok to: {block_number}\n')
        api1 = api_node.api.account_history.enum_virtual_ops(
            block_range_begin=block_number - 1,
            block_range_end=block_number + 1,
            include_reversible=True,
            group_by_block=False,
            operation_begin=0
        )

        for operation in api1['ops']:
            if 'vote_operation' in operation['op']['type']:
                vote_virtual_ops.append(operation)
        file.write(str(vote_virtual_ops)+'\n\n')
        file.close()
        time.sleep(0.5)

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
