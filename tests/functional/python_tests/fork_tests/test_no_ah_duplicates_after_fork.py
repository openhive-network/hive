import test_tools as tt

from ....shared_tools.complex_networks_helper_functions import *


def test_no_duplicates_in_account_history_plugin_after_fork(prepare_basic_networks):
    alpha_net = prepare_basic_networks['Alpha']
    beta_net = prepare_basic_networks['Beta']

    alpha_node = alpha_net.node('ApiNode0')
    beta_node = beta_net.node('ApiNode1')

    # TRIGGER
    # Using network_node_api we block communication between alpha and beta parts.
    # After some time (when irreversible block number increases in both subnetworks) we join alpha and beta part again
    trigger_fork(alpha_net, beta_net)

    # VERIFY
    # Expected behaviour is that nodes in one of subnetworks (random one, alpha or beta) will perform undo and enter
    # live sync. We check there are no duplicates in account_history_api after such scenario (issue #117).
    tt.logger.info('Assert there are no duplicates in account_history.get_ops_in_block after fork...')
    assert_no_duplicates(alpha_node, beta_node)


def trigger_fork(alpha_net, beta_net):
    alpha_node = alpha_net.node('ApiNode0')
    beta_node = beta_net.node('ApiNode1')

    alpha_net.disconnect_from(beta_net)
    tt.logger.info(f'Disconnected {alpha_net} and {beta_net}')

    # wait until head block number increases in both subnetworks
    head_block_during_disconnection = alpha_node.api.database.get_dynamic_global_properties()['head_block_number']
    tt.logger.info(f'head_block_during_disconnection: {head_block_during_disconnection}')
    tt.logger.info('Waiting until head block increases in both subnetworks')
    for node in [alpha_node, beta_node]:
        while True:
            current_head_block = node.api.database.get_dynamic_global_properties()['head_block_number']
            tt.logger.info(f'Head in {node}: {current_head_block}')
            if current_head_block > head_block_during_disconnection:
                break
            node.wait_number_of_blocks(1)

    alpha_net.connect_with(beta_net)
    tt.logger.info(f'Reconnected {alpha_net} and {beta_net}')
