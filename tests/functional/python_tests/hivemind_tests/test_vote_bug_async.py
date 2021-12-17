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

    wallet.api.create_account('initminer', 'alice', '{}')
    wallet.api.create_account('initminer', 'bob', '{}')
    wallet.api.transfer('initminer', 'alice', Asset.Tbd(70008.543), 'avocado')
    wallet.api.transfer('initminer', 'bob', Asset.Tbd(78008.543), 'banana')
    wallet.api.transfer_to_vesting('initminer', 'alice', Asset.Test(1000000))
    wallet.api.transfer_to_vesting('initminer', 'bob', Asset.Test(1000000))

    #operations to prepare bug
    wallet.api.post_comment('alice', 'hello-world2', '', 'xyz2', 'something about world2', 'just nothing2', '{}')
    wallet.api.vote('bob', 'alice', 'hello-world2', 99)

    logger.info(node_test.get_last_block_number())
    node_test.wait_for_block_with_number(20)
    generate_blocks(node_test, 1300)

    # time.sleep(10)
    # hivemind = Hivemind(
    #     database_name='hivemind_pyt',
    #     database_user='dev',
    #     database_password='devdevdev',
    # )
    #
    # task2 = threading.Thread(target=hivemind_run, args=[hivemind, node_test])
    # task2.start()
    http_endpoint = node_test.get_http_endpoint()
    logger.info(http_endpoint)
    input('1320, uruchom i od razu wyłaćz')

    input('Wyłącz hiveminda')

    logger.info('Generacja bloków po w celu obudzenia synca')
    generate_blocks(node_test, 1)
    time.sleep(5)
    task1 = threading.Thread(target=voter, args=[wallet, node_test])
    task1.start()
    time.sleep(5)
    input('1320, uruchom i od razu wyłacz po raz drugi')

    time.sleep(5)
    time.sleep(25)
    logger.info('Wyłącz program')
    time.sleep(10)

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

def hivemind_run(hivemind:Hivemind, node_test):
    hivemind.run(sync_with=node_test, run_server=False)
