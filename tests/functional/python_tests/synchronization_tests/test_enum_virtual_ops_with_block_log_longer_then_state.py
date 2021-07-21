from test_tools import logger


def test_block_api_with_block_log_longer_then_state(world, block_log):
    # PREREQUISITES
    # We need one node and block_log corresponding to state with several active witnesses
    # BLOCK_NUM value depends on provided block_log
    BLOCK_NUM = 100
    replaying_node = world.create_api_node()
    logger.info(f'Replay up to block {BLOCK_NUM} (shorter value then block_log length)...')
    replaying_node.run(replay_from=block_log, stop_at_block=BLOCK_NUM, wait_for_live=False)

    # to reproduce issue with enum_virtual_ops, node must stop replay at BLOCK_NUM and there is no following sync/live sync
    response = replaying_node.api.database.get_dynamic_global_properties()
    head_block_number = response["result"]["head_block_number"]
    last_irreversible_block_num = response["result"]["last_irreversible_block_num"]
    assert head_block_number == BLOCK_NUM
    assert last_irreversible_block_num < head_block_number

    # TRIGGER
    # Query account_history_api.enum_virtual_ops for block range around BLOCK_NUM
    response = replaying_node.api.account_history.enum_virtual_ops(block_range_begin=BLOCK_NUM-5, block_range_end=BLOCK_NUM+5, include_reversible=False)
    ops = response["result"]["ops"]
    block_numbers = [op["block"] for op in ops]
    logger.info("virtual operations found in blocks:")
    logger.info(block_numbers)

    # VERIFY
    # There are virtual operations up to BLOCK_NUM
    for num in range(BLOCK_NUM-5, BLOCK_NUM):
        assert num in block_numbers
