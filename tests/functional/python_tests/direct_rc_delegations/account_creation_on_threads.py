import time
from concurrent.futures import ThreadPoolExecutor
from test_tools import Account, logger, Wallet

def create_account_creation_operation( acc : Account ):
    pattern = ['account_create', {'fee': '0.000 TESTS', 'creator': 'initminer', 'new_account_name': 'receiver2-6',
                                  'owner': {'weight_threshold': 1, 'account_auths': [], 'key_auths': [
                                      ['TST5kUFuULqeDLGERSNEnETLZDAFTNAjocHxZQkuLkERUz1WXUdpz', 1]]},
                                  'active': {'weight_threshold': 1, 'account_auths': [], 'key_auths': [
                                      ['TST5kUFuULqeDLGERSNEnETLZDAFTNAjocHxZQkuLkERUz1WXUdpz', 1]]},
                                  'posting': {'weight_threshold': 1, 'account_auths': [], 'key_auths': [
                                      ['TST5kUFuULqeDLGERSNEnETLZDAFTNAjocHxZQkuLkERUz1WXUdpz', 1]]},
                                  'memo_key': 'TST5kUFuULqeDLGERSNEnETLZDAFTNAjocHxZQkuLkERUz1WXUdpz',
                                  'json_metadata': '{}'}]
    pattern[1]['new_account_name'] = acc.name
    for key_type in ['owner', 'active', 'posting']:
        pattern[1][key_type]['key_auths'][0][0] = acc.public_key
    pattern[1]['memo_key'] = acc.public_key
    return pattern


def test_direct_rc_delegations(wallet, node):
    # logger.info('Start account creations')
    # for number_of_transactions in range(9):
    #     with wallet.in_single_transaction():
    #         for accounts_in_transaction in range(1):
    #             wallet.api.create_account(creator, f'receiver{number_of_transactions}{accounts_in_transaction}', '{}')
    # logger.info('End account creations')

    wallet1 = Wallet(attach_to=node)
    wallet2 = Wallet(attach_to=node)
    wallet3 = Wallet(attach_to=node)

    accounts_thread_1 = []
    accounts_thread_2 = []
    accounts_thread_3 = []

    # wallet.api.create_account('initminer', 'bob', "{}")
    # wallet.api.create_account('initminer', 'alice', "{}")
    # wallet.api.transfer_to_vesting('initminer', 'bob', '100.010 TESTS')
    # wallet.api.transfer_to_vesting('initminer', 'alice', '100.010 TESTS')

    tasks_list = []
    executor = ThreadPoolExecutor(max_workers=3)
    task1 = executor.submit(delegation_rc, wallet1, 1, accounts_thread_1)
    tasks_list.append(task1)

    # delegation_rc(wallet1, 1, accounts_thread_1)
    task2 = executor.submit(delegation_rc, wallet2, 2, accounts_thread_2)
    tasks_list.append(task2)
    #
    task3 = executor.submit(delegation_rc, wallet3, 3, accounts_thread_3)
    tasks_list.append(task3)

    for task in tasks_list:
        try:
            print(task.result())
        except Exception as e:
            print(e)

    dupa1 = accounts_thread_1
    dupa2 = accounts_thread_2
    dupa3 = accounts_thread_3

    api1 = node.api.account_history.get_account_history(
        account='initminer',
        start=-1,
        limit=1000
    )

    api2 = node.api.account_history.enum_virtual_ops(
        block_range_begin=13,
        block_range_end=40,
        include_reversible=True,
        group_by_block=False,
        operation_begin=0
    )

    api3 = node.api.account_history.get_ops_in_block(
        block_num=15,
        only_virtual=False,
        include_reversible=True
    )


    wallet.api.transfer_to_vesting('initminer', 'receiver2-1', '100.010 TESTS')
    wallet.api.delegate_rc('receiver2-1', ['receiver2-2'], 1)

    pass

def delegation_rc(wallet, thread_number, client_accounts):
    logger.info(f'Thread {thread_number} work START')
    logger.info(f'Start account creations---THREAD{thread_number}')
    number_of_accounts_in_one_transaction = 1750
    number_of_transactions = 5
    number_of_accounts = number_of_accounts_in_one_transaction * number_of_transactions
    account_number_absolute = 0
    start = time.perf_counter()
    accounts = Account.create_multiple(number_of_accounts, f'receiver{thread_number}')
    private_keys = [acc.private_key for acc in accounts]

    for i in range(number_of_transactions):
        logger.info('import_keys - start')
        wallet.api.import_keys(list(private_keys[
                i * number_of_accounts_in_one_transaction: i * number_of_accounts_in_one_transaction + number_of_accounts_in_one_transaction]))
        logger.info('import_keys - end')

    print(f'czas na przygotowanie acc: {time.perf_counter() - start :.2f}s')



    trx_pat = {'ref_block_num': 11, 'ref_block_prefix': 3457124627, 'expiration': '1970-01-01T00:00:00', 'extensions': [],'signatures': [],'transaction_id': '0000000000000000000000000000000000000000', 'block_num': 0, 'transaction_num': 0, 'operations': []}

    start = time.perf_counter()

    for i in range(number_of_transactions):
        for acc in list(accounts[i * number_of_accounts_in_one_transaction : i * number_of_accounts_in_one_transaction + number_of_accounts_in_one_transaction]):
            trx_pat['operations'].append(create_account_creation_operation(acc))
        wallet.api.sign_transaction(trx_pat)
        trx_pat['operations'].clear()

    print(f'czas na dodanie accountów: {time.perf_counter() - start :.2f}s')
    print()
    if False:
        for number_of_transaction in range(number_of_transactions):
            with wallet.in_single_transaction():
                start = time.perf_counter()
                # wallet.api.sign_transaction()

                for account_number in range(number_of_accounts_in_one_transaction):
                    account_name = accounts[account_number_absolute].name
                    account_key = accounts[account_number_absolute].public_key
                    wallet.api.create_account_with_keys('initminer', account_name, '{}', account_key, account_key,account_key, account_key)
                    # wallet.api.sign_transaction(create_account_creation_operation())
                    client_accounts.append(account_name)
                    account_number_absolute = account_number_absolute + 1
                logger.info(f'Thread {thread_number}: {time.perf_counter() - start:.2f}s')
    logger.info(f'End account creations----THREAD{thread_number}')
    logger.info(f'Thread {thread_number} work END')
