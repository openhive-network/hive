from __future__ import annotations

from functools import partial

import pytest

from test_tools import complex_networks as ttcn
import test_tools as tt


@pytest.mark.fork_tests_group_1()
def test_fork_2_sub_networks_01(prepare_fork_2_sub_networks_01):
    # start - A network (consists of a 'minority' network(6 witnesses) + a 'majority' network(17 witnesses)) produces blocks

    # - 14 witnesses are disabled( in the 'minority' network )
    # - wait ('blocks_after_disable_witness' + 'blocks_after_disable_witness_2') blocks( using the 'minority' API node )
    # - the network is split into 2 sub networks: 6 witnesses('minority' network) + 3 active witnesses('majority' network)( because 14 witnesses are disabled )
    # - wait 'blocks_after_disconnect' blocks( using 'minority' API node ) - as a result a chain of blocks in the 'minority' network is longer than the 'majority' network
    # - 14 witnesses are enabled( in the 'minority' network )
    # - wait 'blocks_after_enable_witness' blocks( using the 'minority' API node )
    # - 2 sub networks are merged
    # - wait 'N' blocks( using the 'minority' API node ) until both sub networks have the same last irreversible block

    # Finally we have 2 connected networks

    networks_builder = prepare_fork_2_sub_networks_01

    witness_details_part = ttcn.get_part_of_witness_details(networks_builder.witness_names, 6, 14)

    minority_api_node = networks_builder.networks[0].node("FullApiNode0")
    majority_api_node = networks_builder.networks[1].node("FullApiNode1")
    minority_witness_node = networks_builder.networks[0].node("WitnessNode0")

    minority_witness_wallet = tt.Wallet(attach_to=minority_witness_node)

    initminer_private_key = "5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n"
    minority_witness_wallet.api.import_key(initminer_private_key)

    logs = []

    logs.extend(
        (
            ttcn.NodeLog("M", tt.Wallet(attach_to=majority_api_node)),
            ttcn.NodeLog("m", tt.Wallet(attach_to=minority_api_node)),
        )
    )

    _M = logs[0].collector
    _m = logs[1].collector

    blocks_before_disconnect = 10

    blocks_after_disable_witness = 5
    blocks_after_disable_witness_2 = 10

    blocks_after_disconnect = 20

    blocks_after_enable_witness = 5

    tt.logger.info("Before disconnecting")
    cnt = 0
    while True:
        ttcn.wait(1, logs, majority_api_node)

        cnt += 1
        if cnt > blocks_before_disconnect:  # noqa: SIM102
            if ttcn.get_last_irreversible_block_num(_M) == ttcn.get_last_irreversible_block_num(_m):
                break

    assert ttcn.get_last_head_block_number(_M) == ttcn.get_last_head_block_number(_m)
    assert ttcn.get_last_irreversible_block_num(_M) == ttcn.get_last_irreversible_block_num(_m)
    last_lib_01 = ttcn.get_last_irreversible_block_num(_m)

    tt.logger.info(f"Disable {len(witness_details_part)} witnesses - start")
    ttcn.disable_witnesses(minority_witness_wallet, witness_details_part)

    ttcn.wait(blocks_after_disable_witness, logs, minority_api_node)

    assert ttcn.get_last_head_block_number(_M) == ttcn.get_last_head_block_number(_m)
    assert ttcn.get_last_irreversible_block_num(_M) == ttcn.get_last_irreversible_block_num(_m)
    last_lib_01 = ttcn.get_last_irreversible_block_num(_m)

    ttcn.wait(blocks_after_disable_witness_2, logs, minority_api_node)

    assert ttcn.get_last_head_block_number(_M) == ttcn.get_last_head_block_number(_m)
    assert ttcn.get_last_irreversible_block_num(_M) == ttcn.get_last_irreversible_block_num(_m)
    last_lib_02 = ttcn.get_last_irreversible_block_num(_m)

    assert last_lib_01 == last_lib_02

    tt.logger.info("Disconnect sub networks - start")
    ttcn.disconnect_sub_networks(networks_builder.networks)

    ttcn.wait(blocks_after_disconnect, logs, minority_api_node)

    assert ttcn.get_last_head_block_number(_M) < ttcn.get_last_head_block_number(_m)
    assert ttcn.get_last_irreversible_block_num(_M) == ttcn.get_last_irreversible_block_num(_m)
    last_lib_03 = ttcn.get_last_irreversible_block_num(_m)

    assert last_lib_01 == last_lib_03

    tt.logger.info(f"Enable {len(witness_details_part)} witnesses")
    ttcn.enable_witnesses(minority_witness_wallet, witness_details_part)

    ttcn.wait(blocks_after_enable_witness, logs, minority_api_node)

    assert ttcn.get_last_head_block_number(_M) < ttcn.get_last_head_block_number(_m)
    assert ttcn.get_last_irreversible_block_num(_M) == ttcn.get_last_irreversible_block_num(_m)
    last_lib_04 = ttcn.get_last_irreversible_block_num(_m)

    assert last_lib_01 == last_lib_04

    tt.logger.info("Reconnect sub networks")
    ttcn.connect_sub_networks(networks_builder.networks)

    ttcn.wait_for_final_block(
        minority_api_node, logs, [_m, _M], True, partial(ttcn.lib_custom_condition, _M, last_lib_04), False
    )

    ttcn.assert_no_duplicates(minority_api_node, majority_api_node)
