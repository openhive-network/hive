from functools import partial

import shared_tools.complex_networks_helper_functions as sh
import test_tools as tt

def test_fork_3_sub_networks_00(prepare_fork_3_sub_networks_00):
    # start - A network consists of a 'minority_3' network(3 witnesses), a 'minority_4' network(4 witnesses), a 'majority' network(14 witnesses).

    # - the network is split into 3 sub networks: 3 witnesses(the 'minority_3' network), 4 witnesses(the 'minority_4' network), 14 witnesses(the 'majority' network)
    # - wait 'blocks_after_disconnect' blocks( using the 'majority' API node )
    # - 3 sub networks are merged
    # - wait 'N' blocks( using the 'majority' API node ) until all sub networks have the same last irreversible block

    networks_builder = prepare_fork_3_sub_networks_00

    minority_api_node_3 = networks_builder.networks[0].node('ApiNode0')
    minority_api_node_4 = networks_builder.networks[1].node('ApiNode1')
    majority_api_node   = networks_builder.networks[2].node('ApiNode2')

    logs = []

    logs.append(sh.NodeLog("m3", tt.Wallet(attach_to = minority_api_node_3)))
    logs.append(sh.NodeLog("m4", tt.Wallet(attach_to = minority_api_node_4)))
    logs.append(sh.NodeLog("M", tt.Wallet(attach_to = majority_api_node)))

    _m3 = logs[0].collector
    _m4 = logs[1].collector
    _M  = logs[2].collector

    blocks_before_disconnect    = 10
    blocks_after_disconnect     = 10

    tt.logger.info(f'Before disconnecting')
    cnt = 0
    while True:
        sh.wait(1, logs, majority_api_node)

        cnt += 1
        if cnt > blocks_before_disconnect:
            if sh.get_last_irreversible_block_num(_m3) == sh.get_last_irreversible_block_num(_m4) and sh.get_last_irreversible_block_num(_m3) == sh.get_last_irreversible_block_num(_M):
                break

    assert sh.get_last_head_block_number(_M)      == sh.get_last_head_block_number(_m3)
    assert sh.get_last_head_block_number(_M)      == sh.get_last_head_block_number(_m4)

    assert sh.get_last_irreversible_block_num(_M) == sh.get_last_irreversible_block_num(_m3)
    assert sh.get_last_irreversible_block_num(_M) == sh.get_last_irreversible_block_num(_m4)

    tt.logger.info(f'Disconnect sub networks')
    sh.disconnect_sub_networks(networks_builder.networks)

    sh.wait(blocks_after_disconnect, logs, majority_api_node)

    last_lib = sh.get_last_irreversible_block_num(_m3)

    tt.logger.info(f'Reconnect sub networks')
    sh.connect_sub_networks(networks_builder.networks)

    sh.wait_for_final_block(majority_api_node, logs, [_m3, _m4, _M], True, partial(sh.lib_custom_condition, _M, last_lib), False)
