from test_library import Account, logger, World

import os
import time
import concurrent.futures
import random

CONCURRENCY = None


def register_witness(wallet, _account_name, _witness_url, _block_signing_public_key):
    wallet.api.update_witness(
        _account_name,
        _witness_url,
        _block_signing_public_key,
        {"account_creation_fee": "3.000 TESTS", "maximum_block_size": 65536, "sbd_interest_rate": 0}
    )


def self_vote(_witnesses, wallet):
    executor = concurrent.futures.ThreadPoolExecutor(max_workers=CONCURRENCY)
    fs = []
    for w in _witnesses:
        if (isinstance(w, str)):
            account_name = w
        else:
            account_name = w["account_name"]
        future = executor.submit(wallet.api.vote_for_witness, account_name, account_name, 1)
        fs.append(future)
    res = concurrent.futures.wait(fs, timeout=None, return_when=concurrent.futures.ALL_COMPLETED)
    for future in fs:
        future.result()


def prepare_accounts(_accounts, wallet):
    executor = concurrent.futures.ThreadPoolExecutor(max_workers=CONCURRENCY)
    fs = []
    logger.info("Attempting to create {0} accounts".format(len(_accounts)))
    for account in _accounts:
        future = executor.submit(wallet.create_account, account)
        fs.append(future)
    res = concurrent.futures.wait(fs, timeout=None, return_when=concurrent.futures.ALL_COMPLETED)
    for future in fs:
        future.result()


def configure_initial_vesting(_accounts, a, b, _tests, wallet):
    executor = concurrent.futures.ThreadPoolExecutor(max_workers=CONCURRENCY)
    fs = []
    logger.info("Configuring initial vesting for {0} of witnesses".format(str(len(_accounts))))
    for account_name in _accounts:
        value = random.randint(a, b)
        amount = str(value) + ".000 " + _tests
        future = executor.submit(wallet.api.transfer_to_vesting, "initminer", account_name, amount)
        fs.append(future)
    res = concurrent.futures.wait(fs, timeout=None, return_when=concurrent.futures.ALL_COMPLETED)
    for future in fs:
        future.result()


def prepare_witnesses(_witnesses, wallet):
    executor = concurrent.futures.ThreadPoolExecutor(max_workers=CONCURRENCY)
    fs = []
    logger.info("Attempting to prepare {0} of witnesses".format(str(len(_witnesses))))
    for account_name in _witnesses:
        witness = Account(account_name)
        pub_key = witness.public_key
        future = executor.submit(register_witness, wallet, account_name, "https://" + account_name + ".net", pub_key)
        fs.append(future)
    res = concurrent.futures.wait(fs, timeout=None, return_when=concurrent.futures.ALL_COMPLETED)
    for future in fs:
        future.result()


def list_top_witnesses(node):
    start_object = [200277786075957257, '']
    response = node.api.database.list_witnesses(100, 'by_vote_name', start_object)
    return response["result"]["witnesses"]


def print_top_witnesses(witnesses, node):
    witnesses_set = set(witnesses)

    top_witnesses = list_top_witnesses(node)
    position = 1
    for w in top_witnesses:
        owner = w["owner"]
        group = "U"
        if owner in witnesses_set:
            group = "W"

        logger.info(
            "Witness # {0:2d}, group: {1}, name: `{2}', votes: {3}".format(position, group, w["owner"], w["votes"]))
        position = position + 1


def count_producer_reward_operations(node, begin=1, end=500):
    count = {}
    method = 'account_history_api.get_ops_in_block'
    for i in range(begin, end):
        response = node.api.account_history.get_ops_in_block(i, True)
        ops = response["result"]["ops"]
        count[i] = 0
        for op in ops:
            op_type = op["op"]["type"]
            if op_type == "producer_reward_operation":
                count[i] += 1
    return count


def test_no_duplicates_after_fork():
    with World() as world:
        logger.show_debug_logs_on_stdout()  # TODO: Remove this before delivery

        alpha_witness_names = [f'witness{i}-alpha' for i in range(10)]
        beta_witness_names = [f'witness{i}-beta' for i in range(10)]

        # Create first network
        alpha_net = world.create_network('Alpha')
        alpha_net.set_directory('experimental_network')

        init_node = alpha_net.create_init_node()
        alpha_node0 = alpha_net.create_node()
        alpha_node1 = alpha_net.create_node()

        # Create second network
        beta_net = world.create_network('Beta')
        beta_net.set_directory('experimental_network')

        beta_node0 = beta_net.create_node()
        beta_node1 = beta_net.create_node()
        api_node = beta_net.create_node()

        # Create witnesses
        alpha_witness_nodes = [alpha_node0, alpha_node1]
        for name in alpha_witness_names:
            node = random.choice(alpha_witness_nodes)
            node.set_witness(name)

        beta_witness_nodes = [beta_node0, beta_node1]
        for name in beta_witness_names:
            node = random.choice(beta_witness_nodes)
            node.set_witness(name)

        for node in alpha_net.nodes + beta_net.nodes:
            node.config.shared_file_size = '6G'

            node.config.plugin += [
                'network_broadcast_api', 'network_node_api', 'account_history', 'account_history_rocksdb',
                'account_history_api'
            ]

            node.config.log_appender = '{"appender":"stderr","stream":"std_error"} {"appender":"p2p","file":"logs/p2p/p2p.log"}'
            node.config.log_logger = '{"name":"default","level":"debug","appender":"stderr"} {"name":"p2p","level":"warn","appender":"p2p"}'

        api_node.config.plugin.remove('witness')

        # Run
        alpha_net.connect_with(beta_net)

        print('Running networks, waiting for live...')
        alpha_net.run()
        beta_net.run()

        wallet = init_node.attach_wallet()
        beta_wallet = beta_node0.attach_wallet()

        # We are waiting here for block 63, because witness participation is counting
        # by dividing total produced blocks in last 128 slots by 128. When we were waiting
        # too short, for example 42 blocks, then participation equals 42 / 128 = 32.81%.
        # It is not enough, because 33% is required. 63 blocks guarantee, that this
        # requirement is always fulfilled (63 / 128 = 49.22%, which is greater than 33%).
        logger.info('Wait for block 63 (to fulfill required 33% of witness participation)')
        init_node.wait_for_block_with_number(3 * 21)

        # Run original test script

        all_witnesses = alpha_witness_names + beta_witness_names
        random.shuffle(all_witnesses)

        print("Witness state before voting")
        print_top_witnesses(all_witnesses, api_node)
        print(wallet.api.list_accounts())

        prepare_accounts(all_witnesses, wallet)
        configure_initial_vesting(all_witnesses, 500, 1000, "TESTS", wallet)
        prepare_witnesses(all_witnesses, wallet)
        self_vote(all_witnesses, wallet)

        print("Witness state after voting")
        print_top_witnesses(all_witnesses, api_node)
        print(wallet.api.list_accounts())

        # FIXME: It is workaround solution.
        #        Replace it with waiting until new witnesses will be active.
        logger.info('Wait 21 blocks (when new witnesses will be surely active)')
        init_node.wait_number_of_blocks(21)

        # Reason of this wait is to enable moving forward of irreversible block
        # after subnetworks disconnection.
        logger.info('Wait 21 blocks (when every witness sign at least one block)')
        init_node.wait_number_of_blocks(21)

        alpha_net.disconnect_from(beta_net)
        print('Disconnected')

        logger.info('Waiting until irreversible block number increase in both subnetworks')
        irreversible_block_number_during_disconnection = wallet.api.info()["result"]["last_irreversible_block_num"]
        while True:
            current_irreversible_block = wallet.api.info()["result"]["last_irreversible_block_num"]
            if current_irreversible_block > irreversible_block_number_during_disconnection:
                break
            logger.debug(f'Irreversible in {alpha_net}: {current_irreversible_block}')
            alpha_node0.wait_number_of_blocks(1)

        while True:
            current_irreversible_block = beta_wallet.api.info()["result"]["last_irreversible_block_num"]
            if current_irreversible_block > irreversible_block_number_during_disconnection:
                break
            logger.debug(f'Irreversible in {beta_net}: {current_irreversible_block}')
            beta_node0.wait_number_of_blocks(1)

        alpha_net.connect_with(beta_net)
        print('Reconnected')

        for _ in range(50):
            alpha_irreversible = wallet.api.info()["result"]["last_irreversible_block_num"]
            alpha_reward_operations = count_producer_reward_operations(alpha_node0, begin=alpha_irreversible-50, end=alpha_irreversible)
            beta_irreversible = wallet.api.info()["result"]["last_irreversible_block_num"]
            beta_reward_operations = count_producer_reward_operations(beta_node0, begin=beta_irreversible-50, end=beta_irreversible)

            assert sum(i==1 for i in alpha_reward_operations.values()) == 50
            assert sum(i==1 for i in beta_reward_operations.values()) == 50

            alpha_node0.wait_number_of_blocks(1)
