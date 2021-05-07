from test_library import logger, World

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
    with World() as world:
        logger.show_debug_logs_on_stdout()  # TODO: Remove this before delivery

        alpha_witness_names = [f'witness{i}-alpha' for i in range(10)]
        beta_witness_names = [f'witness{i}-beta' for i in range(10)]

        # Create first network
        alpha_net = world.create_network('Alpha')
        alpha_net.set_directory('experimental_network')

        init_node = alpha_net.create_init_node()
        alpha_node0 = alpha_net.create_node()

        # Create witnesses
        for node in alpha_net.nodes:
            node.config.shared_file_size = '6G'

            node.config.plugin += [
                'network_broadcast_api', 'account_history_rocksdb',
                'account_history_api'
            ]

            node.config.log_appender = '{"appender":"stderr","stream":"std_error"} {"appender":"p2p","file":"logs/p2p/p2p.log"}'
            node.config.log_logger = '{"name":"default","level":"debug","appender":"stderr"} {"name":"p2p","level":"warn","appender":"p2p"}'

        # Run
        logger.info('Running network, waiting for live...')
        alpha_net.run()

        wallet = init_node.attach_wallet()

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
