from test_tools import logger


def test_irreversible_block_while_sync(world):
    net = world.create_network()
    init_node = net.create_init_node()
    api_node = net.create_api_node()

    logger.info('Running network, waiting for live sync...')
    net.run()

    # Wait for block number between constants HIVE_MAX_WITNESSES and HIVE_START_MINER_VOTING_BLOCK
    logger.info('Waiting for block number 23...')
    init_node.wait_for_block_with_number(23)

    # Test api_node will enter live sync after restart
    logger.info("Restarting api node...")
    api_node.restart(timeout=90)
