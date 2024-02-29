from __future__ import annotations

import pytest

import shared_tools.complex_networks_helper_functions as sh
import test_tools as tt


@pytest.mark.fork_tests_group_1()
def test_fork_2_sub_networks_00(prepare_fork_2_sub_networks_00):
    # start - A network (consists of a 'minority' network(3 witnesses) + a 'majority' network(18 witnesses) produces blocks

    # - the network is split into 2 sub networks: 6 witnesses(the 'minority' network) + 18 witnesses(the 'majority' network)
    # - wait '10' blocks( using the majority API node ) - as a result a chain of blocks in the 'majority' network is longer than the 'minority' network
    # - 2 sub networks are merged
    # - wait 'N' blocks( using the majority API node ) until both sub networks have the same last irreversible block

    # Finally the 'minority' network gets blocks from the 'majority' network

    networks_builder = prepare_fork_2_sub_networks_00

    minority_api_node = networks_builder.networks[0].node("FullApiNode0")
    majority_api_node = networks_builder.networks[1].node("FullApiNode1")

    logs = []

    logs.extend(
        (
            sh.NodeLog("M", tt.Wallet(attach_to=majority_api_node)),
            sh.NodeLog("m", tt.Wallet(attach_to=minority_api_node)),
        )
    )

    _M = logs[0].collector
    _m = logs[1].collector

    blocks_before_disconnect = 10

    tt.logger.info("Before disconnecting")
    cnt = 0
    while True:
        sh.wait(1, logs, majority_api_node)

        cnt += 1
        if cnt > blocks_before_disconnect:  # noqa: SIM102
            if sh.get_last_irreversible_block_num(_M) == sh.get_last_irreversible_block_num(_m):
                break

    assert sh.get_last_head_block_number(_M) == sh.get_last_head_block_number(_m)
    assert sh.get_last_irreversible_block_num(_M) == sh.get_last_irreversible_block_num(_m)

    tt.logger.info("Disconnect sub networks")
    sh.disconnect_sub_networks(networks_builder.networks)

    sh.wait(10, logs, majority_api_node)

    assert sh.get_last_head_block_number(_M) > sh.get_last_head_block_number(_m)
    assert sh.get_last_irreversible_block_num(_M) > sh.get_last_irreversible_block_num(_m)

    sh.get_last_irreversible_block_num(_M)
    tt.logger.info("Reconnect sub networks")
    sh.connect_sub_networks(networks_builder.networks)

    sh.wait_for_final_block(majority_api_node, logs, [_m, _M], True, sh.lib_true_condition, False)

    sh.assert_no_duplicates(minority_api_node, majority_api_node)
