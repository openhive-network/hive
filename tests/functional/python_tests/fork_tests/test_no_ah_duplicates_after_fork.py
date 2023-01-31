import shared_tools.complex_networks_helper_functions as sh
import test_tools as tt


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
    sh.assert_no_duplicates(alpha_node, beta_node)


def display_current_head_block_number_in_both_networks(info, nodes):
    res = {}

    for node in nodes:
        current_head_block = node.api.database.get_dynamic_global_properties()['head_block_number']
        res[ str(node) ] = current_head_block
        tt.logger.info(f'{info}: {node}: {current_head_block}')

    return res

def trigger_fork(alpha_net, beta_net):
    alpha_node = alpha_net.node('ApiNode0')
    beta_node = beta_net.node('ApiNode1')

    witness_node = alpha_net.node('WitnessNode0')
    tt.logger.info(f'Correct witnesses detection')
    sh.wait_for_specific_witnesses(witness_node, [], [['witness[0-9]+-alpha'], ['witness[0-9]+-beta']])

    head_blocks = display_current_head_block_number_in_both_networks('head_block_before_disconnection', [alpha_node, beta_node])

    alpha_net.disconnect_from(beta_net)
    tt.logger.info(f'Disconnected {alpha_net} and {beta_net}')

    head_blocks = display_current_head_block_number_in_both_networks('head_block_after_disconnection', [alpha_node, beta_node])

    tt.logger.info('Waiting until head block increases in both subnetworks')
    for node in [alpha_node, beta_node]:
        while True:
            old_head_block = head_blocks[ str(node) ]
            current_head_block = node.api.database.get_dynamic_global_properties()['head_block_number']
            tt.logger.info(f'{node}: current_head_block: {current_head_block} old_head_block: {old_head_block}')

            if current_head_block > old_head_block:
                break

            node.wait_number_of_blocks(1)

    alpha_net.connect_with(beta_net)
    tt.logger.info(f'Reconnected {alpha_net} and {beta_net}')
