from test_tools import logger

from .local_tools import assert_no_duplicates


def test_no_duplicates_in_account_history_plugin_after_fork(world_with_witnesses):
    world = world_with_witnesses

    alpha_net = world.network('Alpha')
    beta_net = world.network('Beta')
    alpha_node = alpha_net.node('ApiNode0')
    beta_node = beta_net.node('ApiNode0')

    # TRIGGER
    # Using newtork_node_api we block communication between alpha and beta parts.
    # After some time (when irreversible block number increases in both subnetworks) we join alpha and beta part again
    trigger_fork(alpha_net, beta_net)

    # VERIFY
    # Expected behaviour is that nodes in one of subnetworks (random one, alpha or beta) will perform undo and enter live sync
    # We check there are no duplicates in account_history_api after such scenario (issue #117).
    logger.info("Assert there are no duplicates in account_history.get_ops_in_block after fork...")
    assert_no_duplicates(alpha_node, beta_node)


def trigger_fork(alpha_net, beta_net):
    alpha_node = alpha_net.node('ApiNode0')
    beta_node = beta_net.node('ApiNode0')

    alpha_net.disconnect_from(beta_net)
    logger.info(f'Disconnected {alpha_net} and {beta_net}')

    # wait until irreversible block number increases in both subnetworks
    irreversible_block_number_during_disconnection = alpha_node.api.database.get_dynamic_global_properties()["last_irreversible_block_num"]
    logger.info(f'irreversible_block_number_during_disconnection: {irreversible_block_number_during_disconnection}')
    logger.info('Waiting until irreversible block number increases in both subnetworks')
    for node in [alpha_node, beta_node]:
        while True:
            current_irreversible_block = node.api.database.get_dynamic_global_properties()["last_irreversible_block_num"]
            logger.info(f'Irreversible in {node}: {current_irreversible_block}')
            if current_irreversible_block > irreversible_block_number_during_disconnection:
                break
            node.wait_number_of_blocks(1)

    alpha_net.connect_with(beta_net)
    logger.info(f'Reconnected {alpha_net} and {beta_net}')
