from .local_tools import enable_witnesses, disable_witnesses, get_part_of_witness_details, assert_no_duplicates, wait, fork_log, get_last_head_block_number, get_last_irreversible_block_num
from ..local_tools import connect_sub_networks, disconnect_sub_networks
import test_tools as tt

def test_fork_2_sub_networks_02(prepare_fork_2_sub_networks_02):
    # start - A network (consists of a 'minority' network(6 witnesses) + a 'majority' network(17 witnesses)) produces blocks

    # - 14 witnesses are disabled( in the 'majority' network )
    # - wait `blocks_after_disable_witness` blocks( using the 'minority' API node )
    # - the network is split into 2 sub networks: 6 witnesses('minority' network) + 3 active witnesses('majority' network)( because 14 witnesses are disabled )
    # - wait `blocks_after_disconnect` blocks( using 'minority' API node ) - as a result a chain of blocks in the 'minority' network is longer than the 'majority' network
    # - 2 sub networks are merged
    # - wait `blocks_after_reconnect` blocks( using the 'minority' API node )
    # - 14 witnesses are enabled
    # - wait `blocks_after_enable_witness` blocks( using the 'minority' API node )

    # Finally we have 2 connected networks

    sub_networks_data   = prepare_fork_2_sub_networks_02['sub-networks-data']
    sub_networks        = sub_networks_data[0]
    assert len(sub_networks) == 2

    witness_details     = sub_networks_data[1]
    witness_details_part  = get_part_of_witness_details(witness_details, 6, 14)

    init_wallet         = sub_networks_data[2]

    minority_api_node       = sub_networks[0].node('ApiNode0')
    majority_api_node       = sub_networks[1].node('ApiNode1')
    majority_witness_node   = sub_networks[1].node('WitnessNode1')

    majority_witness_wallet = tt.Wallet(attach_to = majority_witness_node)
    set_expiration_time = 1000
    majority_witness_wallet.api.set_transaction_expiration(set_expiration_time)

    initminer_private_key = '5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n'
    majority_witness_wallet.api.import_key(initminer_private_key)

    logs = []

    logs.append(fork_log("M", majority_witness_wallet))
    logs.append(fork_log("m", tt.Wallet(attach_to = minority_api_node)))

    blocks_before_disconnect        = 10

    blocks_after_disable_witness    = 15

    blocks_after_disconnect         = 20

    blocks_after_enable_witness     = 20

    blocks_after_reconnect          = 5

    tt.logger.info(f'Before disconnecting')
    wait(blocks_before_disconnect, logs, majority_api_node)

    _M = logs[0].collector
    _m = logs[1].collector

    tt.logger.info(f'Disable {len(witness_details_part)} witnesses')
    tt.logger.info(f'{witness_details_part}')
    disable_witnesses(majority_witness_wallet, witness_details_part)

    wait(blocks_after_disable_witness, logs, minority_api_node)

    last_head_m = get_last_head_block_number(_m)
    last_head_M = get_last_head_block_number(_M)

    last_lib_m  = get_last_irreversible_block_num(_m)
    last_lib_M  = get_last_irreversible_block_num(_M)

    tt.logger.info(f'Disconnect sub networks')
    disconnect_sub_networks(sub_networks)

    wait(blocks_after_disconnect, logs, minority_api_node)

    tt.logger.info(f'Reconnect sub networks')
    connect_sub_networks(sub_networks)

    wait(blocks_after_reconnect, logs, minority_api_node)

    tt.logger.info(f'Enable {len(witness_details_part)} witnesses')
    enable_witnesses(majority_witness_wallet, witness_details_part)
    wait(blocks_after_enable_witness, logs, minority_api_node)

    assert last_head_m < get_last_head_block_number(_m)
    assert last_head_M < get_last_head_block_number(_M)

    assert last_lib_m  < get_last_irreversible_block_num(_m)
    assert last_lib_M  < get_last_irreversible_block_num(_M)

    assert get_last_irreversible_block_num(_m) == get_last_irreversible_block_num(_M)