import ctypes
from pathlib import Path
import re
import subprocess
import time
import threading

from test_tools import Asset, Account, communication, logger, Hivemind, World, Wallet
import functional.hive_utils


def test_vote_bug(world:World):
    node_test = world.create_init_node()
    node_test.run()
    wallet = Wallet(attach_to=node_test)

    with wallet.in_single_transaction():
        wallet.api.create_account('initminer', 'alice', '{}')
        wallet.api.create_account('initminer', 'bob', '{}')
        wallet.api.create_account('initminer', 'tommy0', '{}')
        wallet.api.create_account('initminer', 'tommy1', '{}')
        wallet.api.create_account('initminer', 'tommy2', '{}')
        wallet.api.create_account('initminer', 'tommy3', '{}')
        wallet.api.create_account('initminer', 'tommy4', '{}')
        wallet.api.create_account('initminer', 'tommy5', '{}')
        wallet.api.create_account('initminer', 'tommy6', '{}')

    # with wallet.in_single_transaction():
    #     wallet.api.transfer('initminer', 'alice', Asset.Tbd(70008.543), 'avocado')
    #     wallet.api.transfer('initminer', 'bob', Asset.Tbd(700088.543), 'banana')
    #     wallet.api.transfer('initminer', 'tommy0', Asset.Tbd(70008.543), 'blueberry')
    #     wallet.api.transfer('initminer', 'tommy1', Asset.Tbd(70008.543), 'apple')
    #     wallet.api.transfer('initminer', 'tommy2', Asset.Tbd(70008.543), 'cucumber')
    #     wallet.api.transfer('initminer', 'tommy3', Asset.Tbd(70008.543), 'onion')
    #     wallet.api.transfer('initminer', 'tommy4', Asset.Tbd(70008.543), 'grappa')
    #     wallet.api.transfer('initminer', 'tommy5', Asset.Tbd(70008.543), 'orange')
    #     wallet.api.transfer('initminer', 'tommy6', Asset.Tbd(70008.543), 'mandarinian')

    with wallet.in_single_transaction():
        wallet.api.transfer_to_vesting('initminer', 'alice', Asset.Test(1000000))
        wallet.api.transfer_to_vesting('initminer', 'bob', Asset.Test(100000))
        wallet.api.transfer_to_vesting('initminer', 'tommy0', Asset.Test(1000000))
        wallet.api.transfer_to_vesting('initminer', 'tommy1', Asset.Test(1000000))
        wallet.api.transfer_to_vesting('initminer', 'tommy2', Asset.Test(1000000))
        wallet.api.transfer_to_vesting('initminer', 'tommy3', Asset.Test(1000000))
        wallet.api.transfer_to_vesting('initminer', 'tommy4', Asset.Test(1000000))
        wallet.api.transfer_to_vesting('initminer', 'tommy5', Asset.Test(1000000))
        wallet.api.transfer_to_vesting('initminer', 'tommy6', Asset.Test(1000000))

    # operations to prepare bug
    wallet.api.post_comment('alice', 'hello-world2', '', 'xyz2', 'something about world2', 'just nothing2', '{}')
    wallet.api.vote('bob', 'alice', 'hello-world2', 99)

    logger.info(node_test.get_last_block_number())
    node_test.wait_for_block_with_number(10)
    generate_blocks(node_test, 1300)

    hivemind = Hivemind(
        database_name='hivemind_pyt',
        database_user='dev',
        database_password='devdevdev',
    )
    hivemind.set_run_parameters(trial_blocks='0')

    task1 = threading.Thread(target=hivemind_run, args=[hivemind, node_test])
    task1.start()
    task1.join()

    logger.info('Generacja bloków po w celu obudzenia synca')
    generate_blocks(node_test, 1)

    logger.info(f'Ostatni blok po synchronizacji 1 {node_test.get_last_block_number()}')
    task2 = threading.Thread(target=votes_multi, args=[wallet, node_test])
    task2.start()
    time.sleep(10)

    # logger.info(f'Ostatni blok przed generacją w celu podpisania głosowania: {node_test.get_last_block_number()}')
    # generate_blocks(node_test, 1)
    # logger.info(f'Ostatni blok po generacji w celu podpisania głosowania: {node_test.get_last_block_number()}')

    logger.info(f'Ostatni blok przed synchronizacją 2 {node_test.get_last_block_number()}')
    task3 = threading.Thread(target=hivemind_run2, args=[hivemind, node_test])
    task3.start()
    logger.info(f'Ostatni blok przed synchronizacji 2 {node_test.get_last_block_number()}')
    task3.join()
    #generate block to end voting process
    generate_blocks(node_test, 1)
    task2.join()

def hivemind_run(hivemind:Hivemind, node_test):
    hivemind.run(sync_with=node_test, run_server=False, with_time_offset='+3898')
    time.sleep(10)
    hivemind.stop()

def hivemind_run2(hivemind: Hivemind, node_test):
    hivemind.run(sync_with=node_test, run_server=False, with_time_offset='+3898', database_prepare=False)
    time.sleep(10)
    hivemind.stop()

def votes_multi(wallet:Wallet, node_test):
    logger.info('Vote start')
    logger.info(f'Ostatni blok przed głosowaniem: {node_test.get_last_block_number()}')
    with wallet.in_single_transaction():
        wallet.api.vote('initminer', 'alice', 'hello-world2', 99)
        wallet.api.vote('tommy0', 'alice', 'hello-world2', 99)
        wallet.api.vote('tommy1', 'alice', 'hello-world2', 99)
        wallet.api.vote('tommy2', 'alice', 'hello-world2', 99)
        wallet.api.vote('tommy3', 'alice', 'hello-world2', 99)
        wallet.api.vote('tommy4', 'alice', 'hello-world2', 99)
        wallet.api.vote('tommy5', 'alice', 'hello-world2', 99)
        wallet.api.vote('tommy6', 'alice', 'hello-world2', 99)
    logger.info(f'Ostatni blok po głosowaniu: {node_test.get_last_block_number()}')
    logger.info('Vote end')

def generate_blocks(node, number_of_blocks):
    node.api.debug_node.debug_generate_blocks(
        debug_key=Account('initminer').private_key,
        count=number_of_blocks,
        skip=0,
        miss_blocks=0,
        edit_if_needed=True
    )

def voter(wallet:Wallet, node_test):
    logger.info('Vote start')
    logger.info(node_test.get_last_block_number())
    wallet.api.vote('initminer', 'alice', 'hello-world2', 99)
    logger.info(node_test.get_last_block_number())
    logger.info('Vote end')


def voter_with_stop(args, wallet:Wallet, node_test):
    t = threading.currentThread()
    while getattr(t, "do_run", True):
        logger.info('Vote start')
        logger.info(node_test.get_last_block_number())
        wallet.api.vote('initminer', 'alice', 'hello-world2', 99)
        logger.info(node_test.get_last_block_number())
        logger.info('Vote end')
        time.sleep(1)
    print("Stopping as you wish.")

def doit(arg):
    t = threading.currentThread()
    while getattr(t, "do_run", True):
        print ("working on %s" % arg)
        time.sleep(1)
    print("Stopping as you wish.")


def test_vote_bug_multivote(world: World):
    node_test = world.create_init_node()
    node_test.run()
    wallet = Wallet(attach_to=node_test)

##########################################################accounts creation
    client_accounts = []
    number_of_accounts_in_one_transaction = 100
    number_of_transactions = 1
    number_of_accounts = number_of_accounts_in_one_transaction * number_of_transactions
    account_number_absolute = 0

    accounts = Account.create_multiple(number_of_accounts, 'receiver')
    for number_of_transaction in range(number_of_transactions):
        with wallet.in_single_transaction():
            for account_number in range(number_of_accounts_in_one_transaction):
                account_name = accounts[account_number_absolute].name
                account_key = accounts[account_number_absolute].public_key
                wallet.api.create_account_with_keys(creator='initminer', newname=account_name, json_meta='{}',
                                                    owner=account_key, active=account_key, posting=account_key,
                                                    memo=account_key)
                # wallet.api.create_account('initminer', account_name, '{}')
                client_accounts.append(account_name)
                account_number_absolute = account_number_absolute + 1
            logger.info('Group of accounts created')
    logger.info('End account creations')
    logger.info('End of splitting')
#########################################################################end accounts creation
    # with wallet.in_single_transaction():
    #     for x in range(2):
    #         wallet.api.transfer('initminer', client_accounts[x], Asset.Tbd(78.543))

    with wallet.in_single_transaction():
        for y in range(10):
            wallet.api.update_account_auth_key(client_accounts[y], 'active',
                                               'TST8ViK3T9FHbbtQs9Mo5odBM6tSmtFEVCvjEDKNPqKe9U1bJs53f', 60)
            wallet.api.transfer_to_vesting('initminer', client_accounts[y], Asset.Test(100000))

    wallet.api.create_account('initminer', 'alice', '{}')
    wallet.api.create_account('initminer', 'bob', '{}')
    wallet.api.transfer('initminer', 'alice', Asset.Tbd(70008.543), 'avocado')
    wallet.api.transfer('initminer', 'bob', Asset.Tbd(78008.543), 'banana')
    wallet.api.transfer_to_vesting('initminer', 'alice', Asset.Test(1000000))
    wallet.api.transfer_to_vesting('initminer', 'bob', Asset.Test(1000000))

    # operations to prepare bug
    wallet.api.post_comment('alice', 'hello-world2', '', 'xyz2', 'something about world2', 'just nothing2', '{}')
    wallet.api.vote('bob', 'alice', 'hello-world2', 99)

    logger.info(node_test.get_last_block_number())
    node_test.wait_for_block_with_number(20)
    generate_blocks(node_test, 1300)

    hivemind = Hivemind(
        database_name='hivemind_pyt',
        database_user='dev',
        database_password='devdevdev',
    )
    hivemind.set_run_parameters(trial_blocks='0')

    task1 = threading.Thread(target=hivemind_run, args=[hivemind, node_test])
    task1.start()
    task1.join()

    logger.info('Generacja bloków po w celu obudzenia synca')
    generate_blocks(node_test, 1)

    logger.info(f'Ostatni blok po synchronizacji 1 {node_test.get_last_block_number()}')
    task2 = threading.Thread(target=voter_loop, args=[client_accounts, wallet, node_test])
    task2.start()
    time.sleep(20)

    logger.info(f'Ostatni blok przed generacją w celu podpisania głosowania: {node_test.get_last_block_number()}')
    generate_blocks(node_test, 1)
    logger.info(f'Ostatni blok po generacji w celu podpisania głosowania: {node_test.get_last_block_number()}')

    logger.info(f'Ostatni blok przed synchronizacją 2 {node_test.get_last_block_number()}')
    task3 = threading.Thread(target=hivemind_run2, args=[hivemind, node_test])
    task3.start()
    logger.info(f'Ostatni blok po synchronizacji 2 {node_test.get_last_block_number()}')
    task3.join()
    #generate block to end voting process
    generate_blocks(node_test, 1)
    task2.join()

def voter_loop(client_accounts, wallet:Wallet, node_test):
    with wallet.in_single_transaction():
        for x in range(5):
            wallet.api.vote(client_accounts[x], 'alice', f'hello-world{x}', 50)
    logger.info("Wyłącz wszstko")


def test_vote_bug_multivote_manual(world: World):
    node_test = world.create_init_node()
    node_test.run()
    wallet = Wallet(attach_to=node_test)

    with wallet.in_single_transaction():
        wallet.api.create_account('initminer', 'alice', '{}')
        wallet.api.create_account('initminer', 'bob', '{}')
        wallet.api.create_account('initminer', 'tommy0', '{}')
        wallet.api.create_account('initminer', 'tommy1', '{}')
        wallet.api.create_account('initminer', 'tommy2', '{}')
        wallet.api.create_account('initminer', 'tommy3', '{}')
        wallet.api.create_account('initminer', 'tommy4', '{}')
        wallet.api.create_account('initminer', 'tommy5', '{}')
        wallet.api.create_account('initminer', 'tommy6', '{}')

    with wallet.in_single_transaction():
        wallet.api.transfer('initminer', 'alice', Asset.Tbd(70008.543), 'avocado')
        wallet.api.transfer('initminer', 'bob', Asset.Tbd(700088.543), 'banana')
        wallet.api.transfer('initminer', 'tommy0', Asset.Tbd(70008.543), 'blueberry')
        wallet.api.transfer('initminer', 'tommy1', Asset.Tbd(70008.543), 'apple')
        wallet.api.transfer('initminer', 'tommy2', Asset.Tbd(70008.543), 'cucumber')
        wallet.api.transfer('initminer', 'tommy3', Asset.Tbd(70008.543), 'onion')
        wallet.api.transfer('initminer', 'tommy4', Asset.Tbd(70008.543), 'grappa')
        wallet.api.transfer('initminer', 'tommy5', Asset.Tbd(70008.543), 'orange')
        wallet.api.transfer('initminer', 'tommy6', Asset.Tbd(70008.543), 'mandarinian')

    with wallet.in_single_transaction():
        wallet.api.transfer_to_vesting('initminer', 'alice', Asset.Test(1000000))
        wallet.api.transfer_to_vesting('initminer', 'bob', Asset.Test(100000))
        wallet.api.transfer_to_vesting('initminer', 'tommy0', Asset.Test(1000000))
        wallet.api.transfer_to_vesting('initminer', 'tommy1', Asset.Test(1000000))
        wallet.api.transfer_to_vesting('initminer', 'tommy2', Asset.Test(1000000))
        wallet.api.transfer_to_vesting('initminer', 'tommy3', Asset.Test(1000000))
        wallet.api.transfer_to_vesting('initminer', 'tommy4', Asset.Test(1000000))
        wallet.api.transfer_to_vesting('initminer', 'tommy5', Asset.Test(1000000))
        wallet.api.transfer_to_vesting('initminer', 'tommy6', Asset.Test(1000000))

    # operations to prepare bug
    wallet.api.post_comment('alice', 'hello-world2', '', 'xyz2', 'something about world2', 'just nothing2', '{}')
    wallet.api.vote('bob', 'alice', 'hello-world2', 99)

    logger.info(node_test.get_last_block_number())
    node_test.wait_for_block_with_number(10)
    generate_blocks(node_test, 1300)

    http_endpoint = node_test.get_http_endpoint()
    logger.info(http_endpoint)
    input('1320, uruchom i od razu wyłącz hajfmańda')
    logger.info('Generacja bloków po w celu obudzenia synca')

    generate_blocks(node_test, 1)
    time.sleep(5)
    task1 = threading.Thread(target=votes_multi, args=[wallet, node_test])
    task1.start()
    time.sleep(5)
    input('Glosowanie zakończone')
    time.sleep(60)
