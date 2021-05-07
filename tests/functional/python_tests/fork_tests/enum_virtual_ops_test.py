from test_library import logger, World

import os
import time


def test_enum_virtual_ops():
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
        init_node.wait_for_block_with_number(50)

        for i in range(10):
            head_block = wallet.api.info()["result"]["head_block_number"]
            logger.info(f'head_block if {head_block}')
            alpha_node0.api.account_history.e


            
            alpha_node0.wait_number_of_blocks(1)