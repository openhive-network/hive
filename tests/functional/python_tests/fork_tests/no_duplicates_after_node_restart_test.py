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


def test_no_duplicates_after_node_restart():
    with World() as world:
        logger.show_debug_logs_on_stdout()  # TODO: Remove this before delivery

        alpha_witness_names = [f'witness{i}-alpha' for i in range(21)]

        # Create first network
        alpha_net = world.create_network('Alpha')
        alpha_net.set_directory('experimental_network')

        init_node = alpha_net.create_init_node()
        alpha_node0 = alpha_net.create_node()
        for name in alpha_witness_names:
            alpha_node0.set_witness(name)

        # Create witnesses
        for node in alpha_net.nodes:
            node.config.shared_file_size = '6G'

            node.config.plugin += [
                'witness', 'network_broadcast_api', 'account_history_rocksdb',
                'account_history_api'
            ]

            node.config.log_appender = '{"appender":"stderr","stream":"std_error"} {"appender":"p2p","file":"logs/p2p/p2p.log"}'
            node.config.log_logger = '{"name":"default","level":"debug","appender":"stderr"} {"name":"p2p","level":"warn","appender":"p2p"}'

        # Run
        logger.info('Running network, waiting for live...')
        alpha_net.run()

        wallet = init_node.attach_wallet()

        prepare_accounts(alpha_witness_names, wallet)
        configure_initial_vesting(alpha_witness_names, 500, 1000, "TESTS", wallet)
        prepare_witnesses(alpha_witness_names, wallet)
        self_vote(alpha_witness_names, wallet)

        print("Restarting node0...")
        alpha_node0.close()

        init_node.wait_number_of_blocks(20)

        alpha_node0.run()
        init_node.wait_for_block_with_number(55)

        for _ in range(50):
            alpha_irreversible = wallet.api.info()["result"]["last_irreversible_block_num"]
            alpha_reward_operations = count_producer_reward_operations(alpha_node0, begin=alpha_irreversible-50, end=alpha_irreversible)

            assert sum(i==1 for i in alpha_reward_operations.values()) == 50

            alpha_node0.wait_number_of_blocks(1)
