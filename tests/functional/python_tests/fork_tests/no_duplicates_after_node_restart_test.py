from test_library import Account, KeyGenerator, logger, Network

import os
import time


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
            'network_broadcast_api', 'account_history', 'account_history_rocksdb',
            'account_history_api'
        ]

        node.config.log_appender = '{"appender":"stderr","stream":"std_error"} {"appender":"p2p","file":"logs/p2p/p2p.log"}'
        node.config.log_logger = '{"name":"default","level":"debug","appender":"stderr"} {"name":"p2p","level":"warn","appender":"p2p"}'

    init_node.config.enable_stale_production = True
    init_node.config.required_participation = 0

    # Run
    alpha_net.run()

    print("Waiting for network synchronization...")
    alpha_net.wait_for_synchronization_of_all_nodes()

    wallet = alpha_net.attach_wallet()

    time.sleep(60)

    alpha_node0.close()
    alpha_node0.run()
    alpha_node0.wait_for_synchronization()

    for _ in range(40):
        alpha_irreversible = wallet.api.info()["result"]["last_irreversible_block_num"]
        alpha_reward_operations = count_producer_reward_operations(alpha_node0, begin=alpha_irreversible-40, end=alpha_irreversible)

        assert sum(i==1 for i in alpha_reward_operations.values()) == 40

        alpha_node0.wait_number_of_blocks(1)

    # Restart alpha node
