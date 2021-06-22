from test_tools import logger


def count_ops_by_type(node, op_type: str, start: int, limit: int = 50):
    """
    :param op_type: type of operation (ex. 'producer_reward_operation')
    :param start: start queries with this block number
    :param limit: limit queries until start-limit+1
    """
    count = {}
    for i in range(start, start-limit, -1):
        response = node.api.account_history.get_ops_in_block(block_num=i, only_virtual=False)
        ops = response["result"]["ops"]
        count[i] = 0
        for op in ops:
            this_op_type = op["op"]["type"]
            if this_op_type == op_type:
                count[i] += 1
    return count


def test_no_ah_duplicates_after_restart(world_with_witnesses):
    world = world_with_witnesses

    # Test no duplicates after fork
    api_node = world.network('Alpha').node('ApiNode0')

    # TRIGGER
    # We restart one of nodes.
    logger.info("Restarting api node...")
    api_node.restart()
    wallet = api_node.attach_wallet()

    # VERIFY
    # Expected behavour is that after restart node will enter live sync and there are no duplicates in account_history_api (issue #117).
    logger.info("Checking there are no duplicates in account_history.get_ops_in_block after node restart...")
    for _ in range(10):
        alpha_irreversible = wallet.api.info()["result"]["last_irreversible_block_num"]
        api_node_reward_operations = count_ops_by_type(api_node, 'producer_reward_operation', alpha_irreversible, limit=50)

        assert sum(i==1 for i in api_node_reward_operations.values()) == 50

        api_node.wait_number_of_blocks(1)
    logger.info("No there are no duplicates in account_history.get_ops_in_block after node restart...")
