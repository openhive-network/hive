from __future__ import annotations

import pytest

import shared_tools.complex_networks_helper_functions as sh
import test_tools as tt


@pytest.mark.fork_tests_group_3()
def test_no_duplicates_in_account_history_plugin_after_fork(prepare_with_many_witnesses):
    networks_builder = prepare_with_many_witnesses
    alpha_net = networks_builder.networks[0]
    beta_net = networks_builder.networks[1]

    alpha_node = alpha_net.node("FullApiNode0")
    beta_node = beta_net.node("FullApiNode1")

    # TRIGGER
    # Using network_node_api we block communication between alpha and beta parts.
    # After some time (when irreversible block number increases in both subnetworks) we join alpha and beta part again
    trigger_fork(alpha_net, beta_net)

    # VERIFY
    # Expected behaviour is that nodes in one of subnetworks (random one, alpha or beta) will perform undo and enter
    # live sync. We check there are no duplicates in account_history_api after such scenario (issue #117).
    tt.logger.info("Assert there are no duplicates in account_history.get_ops_in_block after fork...")
    sh.assert_no_duplicates(alpha_node, beta_node)


def display_current_head_block_number_in_both_networks(info, nodes):
    res = {}

    for node in nodes:
        current_head_block = node.api.database.get_dynamic_global_properties()["head_block_number"]
        res[str(node)] = current_head_block
        tt.logger.info(f"{info}: {node}: {current_head_block}")

    return res


def trigger_fork(alpha_net, beta_net):
    alpha_node = alpha_net.node("FullApiNode0")
    beta_node = beta_net.node("FullApiNode1")

    witness_node = alpha_net.node("WitnessNode0")
    tt.logger.info("Correct witnesses detection")
    sh.wait_for_specific_witnesses(witness_node, [], [["witness[0-9]+-alpha"], ["witness[0-9]+-beta"]])

    head_blocks = display_current_head_block_number_in_both_networks(
        "head_block_before_disconnection", [alpha_node, beta_node]
    )

    alpha_net.disconnect_from(beta_net)
    tt.logger.info(f"Disconnected {alpha_net} and {beta_net}")

    head_blocks = display_current_head_block_number_in_both_networks(
        "head_block_after_disconnection", [alpha_node, beta_node]
    )

    tt.logger.info("Waiting until head block increases in both subnetworks")
    cnt = 0
    for node in [alpha_node, beta_node]:
        tt.logger.info(f'Processing {"alpha" if cnt == 0 else "beta"} network')
        cnt += 1
        while True:
            old_head_block = head_blocks[str(node)]
            current_head_block = node.api.database.get_dynamic_global_properties()["head_block_number"]
            tt.logger.info(f"{node}: current_head_block: {current_head_block} old_head_block: {old_head_block}")

            if current_head_block > old_head_block:
                break

            node.wait_number_of_blocks(1)

    tt.logger.info("Before connecting networks")
    alpha_net.connect_with(beta_net)
    tt.logger.info(f"Reconnected {alpha_net} and {beta_net}")
