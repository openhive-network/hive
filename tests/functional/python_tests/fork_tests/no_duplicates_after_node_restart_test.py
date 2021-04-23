from test_library import Account, KeyGenerator, logger, Network

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


def test_no_duplicates_after_node_restart():
    logger.show_debug_logs_on_stdout()  # TODO: Remove this before delivery

    alpha_witness_names = [f'witness{i}-alpha' for i in range(10)]
    beta_witness_names = [f'witness{i}-beta' for i in range(10)]

    # Create first network
    alpha_net = Network('Alpha', port_range=range(51000, 52000))
    alpha_net.set_directory('experimental_network')

    init_node = alpha_net.add_node('InitNode')
    alpha_node0 = alpha_net.add_node('Node0')

    # Create witnesses
    init_node.set_witness('initminer')

    for node in alpha_net.nodes:
        # FIXME: This shouldn't be set in all nodes, but let's test it
        node.config.enable_stale_production = False
        node.config.required_participation = 33

        node.config.shared_file_size = '6G'

        node.config.plugin += [
            'network_broadcast_api', 'network_node_api', 'account_history', 'account_history_rocksdb',
            'account_history_api'
        ]

    init_node.config.enable_stale_production = True
    init_node.config.required_participation = 0

    # Run
    alpha_net.run()

    print("Waiting for network synchronization...")
    alpha_net.wait_for_synchronization_of_all_nodes()

    wallet = alpha_net.attach_wallet()

    time.sleep(60)

    for _ in range(40):
        alpha_irreversible = wallet.api.info()["result"]["last_irreversible_block_num"]
        alpha_reward_operations = count_producer_reward_operations(alpha_node0, begin=alpha_irreversible-30, end=alpha_irreversible)

        assert sum(i==1 for i in alpha_reward_operations.values()) == 30

        alpha_node0.wait_number_of_blocks(1)

    # Restart alpha node
