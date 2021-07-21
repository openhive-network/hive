from test_tools import logger


def test_block_api_with_block_log_longer_then_state(world, block_log):
    # PREREQUISITES
    # We need one node and block_log corresponding to state with several active witnesses
    # BLOCK_NUM value depends on provided block_log
    BLOCK_NUM = 80
    replaying_node = world.create_api_node()
    logger.info(f'Replay up to block {BLOCK_NUM} (shorter value then block_log length)...')
    replaying_node.run(replay_from=block_log, stop_at_block=BLOCK_NUM, wait_for_live=False)

    for i in [10, BLOCK_NUM-1, BLOCK_NUM]:
        block = replaying_node.api.block.get_block(block_num=i)
        result = block["result"]
        assert bool(result)

    # TRIGGER
    # Query block_api for block BLOCK_NUM + 1
    block = replaying_node.api.block.get_block(block_num=BLOCK_NUM+1)
    logger.info(block)

    # VERIFY
    # Response without any block
    result = block["result"]
    logger.info("receied blocks:")
    logger.info(result)
    assert bool(result) == False
