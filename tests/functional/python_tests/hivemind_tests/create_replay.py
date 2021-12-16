from  test_tools import constants, World, logger

def test_create_snapshot(world:World):
    node = world.create_init_node()
    node.run()
    http_endpoint = node.get_http_endpoint()
    logger.info(http_endpoint)
    node.wait_for_block_with_number(2000)

