from .local_tools import wait, fork_log, get_last_head_block_number, get_last_irreversible_block_num
from ..local_tools import connect_sub_networks, disconnect_sub_networks
import test_tools as tt

def test_fork_3_sub_networks_00(prepare_fork_3_sub_networks_00):
    # start - A network consists of a 'minority_3' network(3 witnesses), a 'minority_4' network(4 witnesses), a 'majority' network(14 witnesses).

    # - the network is split into 3 sub networks: 3 witnesses(the 'minority_3' network), 4 witnesses(the 'minority_4' network), 14 witnesses(the 'majority' network)
    # - wait 'blocks_after_disconnect' blocks( using the 'majority' API node )
    # - 3 sub networks are merged
    # - wait 'N' blocks( using the 'majority' API node ) until all sub networks have the same last irreversible block

    sub_networks_data   = prepare_fork_3_sub_networks_00['sub-networks-data']
    sub_networks        = sub_networks_data[0]
    assert len(sub_networks) == 3

    minority_api_node_3 = sub_networks[0].node('ApiNode0')
    minority_api_node_4 = sub_networks[1].node('ApiNode1')
    majority_api_node   = sub_networks[2].node('ApiNode2')

    logs = []

    logs.append(fork_log("m3", tt.Wallet(attach_to = minority_api_node_3)))
    logs.append(fork_log("m4", tt.Wallet(attach_to = minority_api_node_4)))
    logs.append(fork_log("M", tt.Wallet(attach_to = majority_api_node)))

    _m3 = logs[0].collector
    _m4 = logs[1].collector
    _M  = logs[2].collector

    blocks_before_disconnect    = 10
    blocks_after_disconnect     = 10

    tt.logger.info(f'Before disconnecting')
    wait(blocks_before_disconnect, logs, majority_api_node)

    assert get_last_head_block_number(_M)      == get_last_head_block_number(_m3)
    assert get_last_head_block_number(_M)      == get_last_head_block_number(_m4)

    assert get_last_irreversible_block_num(_M) == get_last_irreversible_block_num(_m3)
    assert get_last_irreversible_block_num(_M) == get_last_irreversible_block_num(_m4)

    tt.logger.info(f'Disconnect sub networks')
    disconnect_sub_networks(sub_networks)

    wait(blocks_after_disconnect, logs, majority_api_node)

    last_lib = get_last_irreversible_block_num(_m3)

    tt.logger.info(f'Reconnect sub networks')
    connect_sub_networks(sub_networks)

    while True:
        wait(1, logs, majority_api_node)

        if get_last_irreversible_block_num(_M) > last_lib:
            if get_last_irreversible_block_num(_M) == get_last_irreversible_block_num(_m3) and get_last_irreversible_block_num(_M) == get_last_irreversible_block_num(_m4):
                break
