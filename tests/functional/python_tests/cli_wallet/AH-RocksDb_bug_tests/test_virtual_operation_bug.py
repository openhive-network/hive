import time
from concurrent.futures import ThreadPoolExecutor

from test_tools import Asset, logger, Wallet, World


def test_virtual_operation_bug(world: World):
    network = world.create_network()
    init_node = network.create_init_node()
    api_node = network.create_api_node()
    network.run()
    wallet_initnode = Wallet(attach_to=init_node)

    number_of_accounts = 11
    logger.info('Start of account creation')
    accounts = get_accounts_name(wallet_initnode.create_accounts(number_of_accounts, 'receiver'))
    logger.info('End of account creation')

    with wallet_initnode.in_single_transaction():
        for account in accounts:
            wallet_initnode.api.transfer_to_vesting('initminer', account, Asset.Test(2000000))

    wallet_initnode.api.post_comment('receiver-0', 'hello-world0', '', 'xyz', 'something', 'just nothing', '{}')

    tasks_list = []
    executor = ThreadPoolExecutor(max_workers=2)
    tasks_list.append(executor.submit(enum_testing, api_node))
    tasks_list.append(executor.submit(voting, wallet_initnode, init_node))

    for thread_number in tasks_list:
        thread_number.result()


def voting(wallet_initnode, init_node):
    logger.info('Start voting operation')
    init_node.wait_for_block_with_number(8) #Is necessary because we are synchronizing threads with each other.
    wallet_initnode.api.vote('receiver-10', 'receiver-0', 'hello-world0', 97)


def enum_testing(api_node):
    logger.info('Virtual operation bug thread start')
    block_number = 8
    api_node.wait_for_block_with_number(block_number) #Is necessary because we are synchronizing threads with each other.
    time.sleep(0.5) #Necessary to ensure that a vote will be performed first, but there will be no time for build another block.

    vote_virtual_ops = []
    api1 = api_node.api.account_history.enum_virtual_ops(
        block_range_begin=block_number - 1,
        block_range_end=block_number + 1,
        include_reversible=True,
        group_by_block=False,
        operation_begin=0
    )

    for operation in api1['ops']:
        if 'vote_operation' in operation['op']['type'] and f'receiver-0' in operation['op']['value']['author']:
            vote_virtual_ops.append(operation)
    assert len(vote_virtual_ops) == 0


def get_accounts_name(accounts):
    accounts_names = []
    for account_number in range(len(accounts)):
        accounts_names.append(accounts[account_number].name)
    return accounts_names
