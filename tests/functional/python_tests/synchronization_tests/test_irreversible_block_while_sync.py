from test_tools import logger


def test_irreversible_block_while_sync(world):
    net = world.create_network()
    init_node = net.create_init_node()
    api_node = net.create_api_node()

    logger.info('Running network, waiting for live sync...')
    net.run()

    # PREREQUISITES
    # We need to be before head block number HIVE_START_MINER_VOTING_BLOCK constant (30 in testnet). Also head block number
    # must be greater then HIVE_MAX_WITNESSES (21 in both testnet and mainnet).
    logger.info('Waiting for block number 23...')
    init_node.wait_for_block_with_number(23)

    # TRIGGER
    # Node restart at block n, HIVE_MAX_WITNESSES < n < HIVE_START_MINER_VOTING_BLOCK.    
    logger.info("Restarting api node...")
    api_node.restart(timeout=90)

    # VERIFY
    # No crash.
