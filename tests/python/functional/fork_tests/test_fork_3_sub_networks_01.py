from __future__ import annotations

from functools import partial

import pytest

from test_tools import complex_networks as ttcn
import test_tools as tt


@pytest.mark.fork_tests_group_2()
@pytest.mark.flaky(reruns=3)
def test_fork_3_sub_networks_01(prepare_fork_3_sub_networks_01: ttcn.NetworksBuilder):
    # start - A network consists of a 'minority_7a' network(7 witnesses), a 'minority_7b' network(7 witnesses), a 'minority_7c' network(7 witnesses).

    # - the network is split into 2 sub networks: (7 witnesses(the 'minority_7a' network)) and (7 witnesses(the 'minority_7b' network), 7 witnesses(the 'minority_7c' network))
    # - wait 'blocks_after_disconnect' blocks( using the 'minority_api_node_7a' API node )
    # - 2 sub networks are merged
    # - wait 'N' blocks( using the 'minority_api_node_7a' API node ) until all sub networks have the same last irreversible block

    networks_builder = prepare_fork_3_sub_networks_01

    minority_api_node_7a = networks_builder.networks[0].node("ApiNode0")
    minority_api_node_7b = networks_builder.networks[1].node("ApiNode1")
    minority_api_node_7c = networks_builder.networks[2].node("ApiNode2")

    logs = [
        ttcn.NodeLog("m7a", tt.Wallet(attach_to=minority_api_node_7a)),
        ttcn.NodeLog("m7b", tt.Wallet(attach_to=minority_api_node_7b)),
        ttcn.NodeLog("m7c", tt.Wallet(attach_to=minority_api_node_7c)),
    ]

    _m7a = logs[0].collector
    _m7b = logs[1].collector
    _m7c = logs[2].collector

    blocks_before_disconnect = 10
    blocks_after_disconnect = 10

    tt.logger.info("Before disconnecting")
    cnt = 0
    while True:
        ttcn.wait(1, logs, minority_api_node_7a)

        cnt += 1
        if cnt > blocks_before_disconnect:  # noqa: SIM102
            if ttcn.get_last_irreversible_block_num(_m7a) == ttcn.get_last_irreversible_block_num(
                _m7b
            ) and ttcn.get_last_irreversible_block_num(_m7a) == ttcn.get_last_irreversible_block_num(_m7c):
                break

    assert ttcn.get_last_head_block_number(_m7a) == ttcn.get_last_head_block_number(_m7b)
    assert ttcn.get_last_head_block_number(_m7a) == ttcn.get_last_head_block_number(_m7c)

    assert ttcn.get_last_irreversible_block_num(_m7a) == ttcn.get_last_irreversible_block_num(_m7b)
    assert ttcn.get_last_irreversible_block_num(_m7a) == ttcn.get_last_irreversible_block_num(_m7c)

    tt.logger.info('Disconnect "minority_7a" sub network')
    networks_builder.networks[0].disconnect_from(networks_builder.networks[1])
    networks_builder.networks[0].disconnect_from(networks_builder.networks[2])

    ttcn.wait(blocks_after_disconnect, logs, minority_api_node_7a)

    last_lib_a = ttcn.get_last_irreversible_block_num(_m7a)
    last_lib_b = ttcn.get_last_irreversible_block_num(_m7b)
    last_lib_c = ttcn.get_last_irreversible_block_num(_m7c)

    assert last_lib_a == last_lib_b
    assert last_lib_b == last_lib_c

    tt.logger.info('Reconnect "minority_7a" sub network')
    networks_builder.networks[0].connect_with(networks_builder.networks[1])
    networks_builder.networks[0].connect_with(networks_builder.networks[2])

    ttcn.wait_for_final_block(
        minority_api_node_7a, logs, [_m7a, _m7b, _m7c], True, partial(ttcn.lib_custom_condition, _m7a, last_lib_a), False
    )
