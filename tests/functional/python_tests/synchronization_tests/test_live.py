from test_tools import logger


def test_irreversible_block_while_sync(world):
    net = world.create_network()
    init_node = net.create_init_node()
    api_node = net.create_api_node()
    #api_node2 = net.create_api_node()

    logger.info('Running network, waiting for live sync...')
    net.run()

    logger.info('Waiting for block number 7 on init_node...')
    init_node.wait_for_block_with_number(7)
    
    logger.info('Waiting for block number 11 on api_node...')
    api_node.wait_for_block_with_number(11)
