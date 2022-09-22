from .local_tools import connect_sub_networks, disconnect_sub_networks, wait, fork_log, get_last_head_block_number, get_last_irreversible_block_num
import test_tools as tt

def test_fork_3_sub_networks_01(prepare_fork_3_sub_networks_01):
    # start - A network consists of a 'minority_7a' network(7 witnesses), a 'minority_7b' network(7 witnesses), a 'minority_7c' network(7 witnesses).

    # - the network is split into 2 sub networks: (7 witnesses(the 'minority_7a' network)) and (7 witnesses(the 'minority_7b' network), 7 witnesses(the 'minority_7c' network))
    # - wait 'blocks_after_disconnect' blocks( using the 'minority_api_node_7a' API node )
    # - 2 sub networks are merged
    # - wait 'N' blocks( using the 'minority_api_node_7a' API node ) until all sub networks have the same last irreversible block

    sub_networks_data   = prepare_fork_3_sub_networks_01['sub-networks-data']
    sub_networks        = sub_networks_data[0]
    assert len(sub_networks) == 3

    minority_api_node_7a = sub_networks[0].node('ApiNode0')
    minority_api_node_7b = sub_networks[1].node('ApiNode1')
    minority_api_node_7c = sub_networks[2].node('ApiNode2')

    logs = []

    logs.append(fork_log("m7a", tt.Wallet(attach_to = minority_api_node_7a)))
    logs.append(fork_log("m7b", tt.Wallet(attach_to = minority_api_node_7b)))
    logs.append(fork_log("m7c", tt.Wallet(attach_to = minority_api_node_7c)))

    _m7a = logs[0].collector
    _m7b = logs[1].collector
    _m7c = logs[2].collector

    blocks_before_disconnect    = 10
    blocks_after_disconnect     = 10

    tt.logger.info(f'Before disconnecting')
    wait(blocks_before_disconnect, logs, minority_api_node_7a)

    assert get_last_head_block_number(_m7a)      == get_last_head_block_number(_m7b)
    assert get_last_head_block_number(_m7a)      == get_last_head_block_number(_m7c)

    assert get_last_irreversible_block_num(_m7a) == get_last_irreversible_block_num(_m7b)
    assert get_last_irreversible_block_num(_m7a) == get_last_irreversible_block_num(_m7c)

    tt.logger.info(f'Disconnect "minority_7a" sub network')
    sub_networks[0].disconnect_from(sub_networks[1])
    sub_networks[0].disconnect_from(sub_networks[2])

    wait(blocks_after_disconnect, logs, minority_api_node_7a)

    last_lib_a = get_last_irreversible_block_num(_m7a)
    last_lib_b = get_last_irreversible_block_num(_m7b)
    last_lib_c = get_last_irreversible_block_num(_m7c)

    assert last_lib_a == last_lib_b
    assert last_lib_b == last_lib_c

    tt.logger.info(f'Reconnect "minority_7a" sub network')
    sub_networks[0].connect_with(sub_networks[1])
    sub_networks[0].connect_with(sub_networks[2])

    while True:
        wait(1, logs, minority_api_node_7a)

        if get_last_irreversible_block_num(_m7a) > last_lib_a:
            if get_last_irreversible_block_num(_m7a) == get_last_irreversible_block_num(_m7b) and get_last_irreversible_block_num(_m7a) == get_last_irreversible_block_num(_m7c):
                break
