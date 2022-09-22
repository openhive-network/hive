from .local_tools import enable_witnesses, disable_witnesses, get_part_of_witness_details, assert_no_duplicates, connect_sub_networks, disconnect_sub_networks, wait, fork_log, get_last_head_block_number, get_last_irreversible_block_num
import test_tools as tt

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

    sub_networks_data   = prepare_fork_2_sub_networks_01['sub-networks-data']
    sub_networks        = sub_networks_data[0]
    assert len(sub_networks) == 2

    witness_details     = sub_networks_data[1]
    witness_details_part  = get_part_of_witness_details(witness_details, 6, 14)

    init_wallet         = sub_networks_data[2]

    minority_api_node   = sub_networks[0].node('ApiNode0')
    majority_api_node   = sub_networks[1].node('ApiNode1')
    minority_witness_node   = sub_networks[0].node('WitnessNode0')

    minority_witness_wallet = tt.Wallet(attach_to = minority_witness_node)

    initminer_private_key = '5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n'
    minority_witness_wallet.api.import_key(initminer_private_key)

    logs = []

    logs.append(fork_log("M", tt.Wallet(attach_to = majority_api_node)))
    logs.append(fork_log("m", tt.Wallet(attach_to = minority_api_node)))

    blocks_before_disconnect        = 5

    blocks_after_disable_witness    = 5
    blocks_after_disable_witness_2  = 10

    blocks_after_disconnect         = 20

    blocks_after_enable_witness     = 5

    tt.logger.info(f'Before disconnecting')
    wait(blocks_before_disconnect, logs, majority_api_node)

    _M = logs[0].collector
    _m = logs[1].collector

    assert get_last_head_block_number(_M)      == get_last_head_block_number(_m)
    assert get_last_irreversible_block_num(_M) == get_last_irreversible_block_num(_m)
    last_lib_01                                 = get_last_irreversible_block_num(_m)

    tt.logger.info(f'Disable {len(witness_details_part)} witnesses - start')
    disable_witnesses(minority_witness_wallet, witness_details_part)

    wait(blocks_after_disable_witness, logs, minority_api_node)

    assert get_last_head_block_number(_M)      == get_last_head_block_number(_m)
    assert get_last_irreversible_block_num(_M) == get_last_irreversible_block_num(_m)
    last_lib_01                                 = get_last_irreversible_block_num(_m)

    wait(blocks_after_disable_witness_2, logs, minority_api_node)

    assert get_last_head_block_number(_M)      == get_last_head_block_number(_m)
    assert get_last_irreversible_block_num(_M) == get_last_irreversible_block_num(_m)
    last_lib_02                                 = get_last_irreversible_block_num(_m)

    assert last_lib_01 == last_lib_02

    tt.logger.info(f'Disconnect sub networks - start')
    disconnect_sub_networks(sub_networks)

    wait(blocks_after_disconnect, logs, minority_api_node)

    assert get_last_head_block_number(_M)      <  get_last_head_block_number(_m)
    assert get_last_irreversible_block_num(_M) == get_last_irreversible_block_num(_m)
    last_lib_03                                 = get_last_irreversible_block_num(_m)

    assert last_lib_01 == last_lib_03

    tt.logger.info(f'Enable {len(witness_details_part)} witnesses')
    enable_witnesses(minority_witness_wallet, witness_details_part)

    wait(blocks_after_enable_witness, logs, minority_api_node)

    assert get_last_head_block_number(_M)      <  get_last_head_block_number(_m)
    assert get_last_irreversible_block_num(_M) == get_last_irreversible_block_num(_m)
    last_lib_04                                 = get_last_irreversible_block_num(_m)

    assert last_lib_01 == last_lib_04

    tt.logger.info(f'Reconnect sub networks')
    connect_sub_networks(sub_networks)

    while True:
        wait(1, logs, minority_api_node)

        if get_last_irreversible_block_num(_M) == get_last_irreversible_block_num(_m):
            break

    assert_no_duplicates(minority_api_node, majority_api_node)
