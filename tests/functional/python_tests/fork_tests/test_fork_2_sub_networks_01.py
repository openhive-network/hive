from .local_tools import enable_witnesses, disable_witnesses, get_part_of_witness_details, assert_no_duplicates, connect_sub_networks, disconnect_sub_networks, wait, fork_log, get_last_head_block_number, get_last_irreversible_block_num
import test_tools as tt

def test_fork_2_sub_networks_01(prepared_sub_networks_6_17):
    # start - A network (consists of a 'minority' network(6 witnesses) + a 'majority' network(17 witnesses)) produces blocks

    # - 14 witnesses are disabled( in the 'minority' network )
    # - wait '6 blocks( using the 'minority' API node )
    # - the network is splitted into 2 sub networks: 6 witnesses('minority' network) + 3 active witnesses('majority' network)( because 14 witnesses are disabled )
    # - wait 20 blocks( using 'minority' API node ) - as a result a chain of blocks in the 'minority' network is longer than the 'majority' network
    # - 14 witnesses are enabled( in the 'minority' network )
    # - wait 5 blocks( using the 'minority' API node )
    # - 2 sub networks are merged
    # - wait 'N' blocks( using the 'minority' API node ) until both sub networks have the same last irreversible block

    # Finally 'majority' network gets blocks from 'minority' network

    sub_networks_data   = prepared_sub_networks_6_17['sub-networks-data']
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

    tt.logger.info(f'Before disconnecting')
    wait(1, logs, majority_api_node)

    _M = logs[0].collector
    _m = logs[1].collector

    assert get_last_head_block_number(_M)      == get_last_head_block_number(_m)
    assert get_last_irreversible_block_num(_M) == get_last_irreversible_block_num(_m)
    last_lib_01                                 = get_last_irreversible_block_num(_m)

    tt.logger.info(f'Disable {len(witness_details_part)} witnesses - start')
    disable_witnesses(minority_witness_wallet, witness_details_part)
    wait(1, logs, minority_api_node)

    assert get_last_head_block_number(_M)      == get_last_head_block_number(_m)
    assert get_last_irreversible_block_num(_M) == get_last_irreversible_block_num(_m)
    last_lib_01                                 = get_last_irreversible_block_num(_m)

    wait(5, logs, minority_api_node)

    assert get_last_head_block_number(_M)      == get_last_head_block_number(_m)
    assert get_last_irreversible_block_num(_M) == get_last_irreversible_block_num(_m)
    last_lib_02                                 = get_last_irreversible_block_num(_m)

    assert last_lib_01 == last_lib_02

    tt.logger.info(f'Disconnect sub networks - start')
    disconnect_sub_networks(sub_networks)
    wait(20, logs, minority_api_node)

    assert get_last_head_block_number(_M)      <  get_last_head_block_number(_m)
    assert get_last_irreversible_block_num(_M) == get_last_irreversible_block_num(_m)
    last_lib_03                                 = get_last_irreversible_block_num(_m)

    assert last_lib_01 == last_lib_03

    tt.logger.info(f'Enable {len(witness_details_part)} witnesses')
    enable_witnesses(minority_witness_wallet, witness_details_part)
    wait(5, logs, minority_api_node)

    assert get_last_head_block_number(_M)      <  get_last_head_block_number(_m)
    assert get_last_irreversible_block_num(_M) == get_last_irreversible_block_num(_m)
    last_head_block_minority                    = get_last_head_block_number(_m)
    last_lib_04                                 = get_last_irreversible_block_num(_m)

    assert last_lib_01 == last_lib_04

    old_minority_last_head_block = get_last_head_block_number(_m)
    tt.logger.info(f'Reconnect sub networks')
    connect_sub_networks(sub_networks)
    while True:
        wait(1, logs, minority_api_node)

        minority_last_head_block = get_last_head_block_number(_m)

        assert old_minority_last_head_block + 1 == minority_last_head_block

        if get_last_irreversible_block_num(_M) == get_last_irreversible_block_num(_m):
            wait(1, logs, minority_api_node)
            break

    assert_no_duplicates(minority_api_node, majority_api_node)
