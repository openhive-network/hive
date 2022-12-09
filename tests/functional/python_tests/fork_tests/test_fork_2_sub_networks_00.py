from shared_tools.complex_networks_helper_functions import *
import test_tools as tt

def test_fork_2_sub_networks_00(prepare_fork_2_sub_networks_00):
    # start - A network (consists of a 'minority' network(3 witnesses) + a 'majority' network(18 witnesses) produces blocks

    # - the network is split into 2 sub networks: 6 witnesses(the 'minority' network) + 18 witnesses(the 'majority' network)
    # - wait '10' blocks( using the majority API node ) - as a result a chain of blocks in the 'majority' network is longer than the 'minority' network
    # - 2 sub networks are merged
    # - wait 'N' blocks( using the majority API node ) until both sub networks have the same last irreversible block

    # Finally the 'minority' network gets blocks from the 'majority' network

    sub_networks_data   = prepare_fork_2_sub_networks_00['sub-networks-data']
    sub_networks        = sub_networks_data[0]
    assert len(sub_networks) == 2

    minority_api_node = sub_networks[0].node('ApiNode0')
    majority_api_node = sub_networks[1].node('ApiNode1')

    logs = []

    logs.append(NodeLog("M", tt.Wallet(attach_to = majority_api_node)))
    logs.append(NodeLog("m", tt.Wallet(attach_to = minority_api_node)))

    _M = logs[0].collector
    _m = logs[1].collector

    blocks_before_disconnect        = 10

    tt.logger.info(f'Before disconnecting')
    cnt = 0
    while True:
        wait(1, logs, majority_api_node)

        cnt += 1
        if cnt > blocks_before_disconnect:
            if get_last_irreversible_block_num(_M) == get_last_irreversible_block_num(_m):
                break

    assert get_last_head_block_number(_M)      == get_last_head_block_number(_m)
    assert get_last_irreversible_block_num(_M) == get_last_irreversible_block_num(_m)

    tt.logger.info(f'Disconnect sub networks - start')
    disconnect_sub_networks(sub_networks)

    wait(10, logs, majority_api_node)

    assert get_last_head_block_number(_M)      > get_last_head_block_number(_m)
    assert get_last_irreversible_block_num(_M) > get_last_irreversible_block_num(_m)

    old_majority_last_lib = get_last_irreversible_block_num(_M)
    tt.logger.info(f'Reconnect sub networks - start')
    connect_sub_networks(sub_networks)

    wait_for_final_block(majority_api_node, logs, [_m, _M], True, lib_true_condition, False)

    assert_no_duplicates(minority_api_node, majority_api_node)
