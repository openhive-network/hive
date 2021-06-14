from test_tools import logger, World
import pytest


@pytest.mark.timeout(90)
def test_irreversible_block_while_sync():
    with World() as world:
        net = world.create_network('Net')
        init_node = net.create_init_node()
        api_node = net.create_node()

        # Prepare config
        for node in net.nodes:
            node.config.shared_file_size = '54M'

        logger.info('Running network, waiting for live sync...')
        net.run()

        logger.info(f'Waiting for block number {23}...')
        init_node.wait_for_block_with_number(23)

        # Test api_node will enter live sync after restart
        logger.info("Restarting api node...")
        api_node.close()
        api_node.run(use_existing_config=False)
        # TODO use node.wait_number_of_blocks(1, timeout=20) when test_tools submodule is updated
        api_node.wait_number_of_blocks(1)