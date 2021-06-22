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


def test_no_ah_duplicates_after_fork(world_with_witnesses):
    world = world_with_witnesses

    # Test no duplicates after fork
    alpha_net = world.network('Alpha')
    beta_net = world.network('Beta')
    alpha_node = alpha_net.node('ApiNode0')
    beta_node = beta_net.node('ApiNode0')

    alpha_wallet = alpha_node.attach_wallet()
    beta_wallet = beta_node.attach_wallet()

    # TRIGGER
    # Using newtork_node_api we block communication between alpha and beta parts.
    # After some time (when irreversible block number increases in both subnetworks) we join alpha and beta part again
    alpha_net.disconnect_from(beta_net)
    logger.info('Disconnected alpha_net from beta_net')

    irreversible_block_number_during_disconnection = alpha_wallet.api.info()["result"]["last_irreversible_block_num"]
    logger.info(f'irreversible_block_number_during_disconnection: {irreversible_block_number_during_disconnection}')
    logger.info('Waiting until irreversible block number increase in both subnetworks')
    for node in [alpha_node, beta_node]:
        while True:
            current_irreversible_block = node.api.database.get_dynamic_global_properties()["result"]["last_irreversible_block_num"]
            logger.info(f'Irreversible in {node}: {current_irreversible_block}')
            if current_irreversible_block > irreversible_block_number_during_disconnection:
                break
            node.wait_number_of_blocks(1)

    alpha_net.connect_with(beta_net)
    logger.info('Reconnected')


    # VERIFY
    # Expected behaviour is that nodes in one of subnetworks (random one, alpha or beta) will perform undo and synchronize with other subnetwork.
    # We check there are no duplicates in account_history_api after such scenario (issue #117).
    logger.info("Checking there are no duplicates in account_history.get_ops_in_block after fork...")
    for _ in range(10):
        alpha_irreversible = alpha_wallet.api.info()["result"]["last_irreversible_block_num"]
        alpha_reward_operations = count_ops_by_type(alpha_node, 'producer_reward_operation', alpha_irreversible, limit=50)
        beta_irreversible = beta_wallet.api.info()["result"]["last_irreversible_block_num"]
        beta_reward_operations = count_ops_by_type(beta_node, 'producer_reward_operation', beta_irreversible, limit=50)

        assert sum(i==1 for i in alpha_reward_operations.values()) == 50
        assert sum(i==1 for i in beta_reward_operations.values()) == 50

        alpha_node.wait_number_of_blocks(1)
    logger.info("No there are no duplicates in account_history.get_ops_in_block after fork...")
