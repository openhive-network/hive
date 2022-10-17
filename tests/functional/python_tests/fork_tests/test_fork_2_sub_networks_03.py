from .local_tools import enable_witnesses, disable_witnesses, get_part_of_witness_details, assert_no_duplicates, wait, fork_log, get_last_head_block_number, get_last_irreversible_block_num
from ..local_tools import connect_sub_networks, disconnect_sub_networks
import test_tools as tt
from time import sleep

def test_fork_2_sub_networks_04(prepare_fork_2_sub_networks_03):
    # start - A network (consists of a 'minority' network(3 witnesses) + a 'majority' network(18 witnesses)) produces blocks

    # - the network is split into 2 sub networks: 3 witnesses('minority' network) + 18 witnesses('majority' network)
    # - wait `blocks_after_disconnect` blocks( using 'majority' API node )
    # - 14 witnesses are disabled( in the 'majority' network )
    # - wait `blocks_after_disable_witness` blocks( using the 'minority' API node )
    # - 2 sub networks are merged
    # - wait 20 seconds
    # - 14 witnesses are enabled
    # - wait `blocks_after_enable_witness` blocks( using the 'minority' API node )

    # Finally we have 2 connected networks. The 'majority' network is shorter, but contains LIB which is higher than LIB of the 'minority' sub network,
    # so merging sub networks is done as soon as possible.

    sub_networks_data   = prepare_fork_2_sub_networks_03['sub-networks-data']
    sub_networks        = sub_networks_data[0]
    assert len(sub_networks) == 2

    witness_details     = sub_networks_data[1]
    witness_details_part  = get_part_of_witness_details(witness_details, 4, 16)

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

    _M = logs[0].collector
    _m = logs[1].collector

    blocks_before_disconnect        = 5
    blocks_after_disconnect         = 2
    blocks_after_disable_witness    = 10
    blocks_after_enable_witness   = 20

    tt.logger.info(f'Before disconnecting')

    wait(blocks_before_disconnect, logs, majority_api_node)

    tt.logger.info(f'Disconnect sub networks')
    disconnect_sub_networks(sub_networks)

    wait(blocks_after_disconnect, logs, majority_api_node)

    tt.logger.info(f'Disable {len(witness_details_part)} witnesses')
    tt.logger.info(f'{witness_details_part}')
    disable_witnesses(majority_witness_wallet, witness_details_part)
    tt.logger.info(f'Witnesses are disabled')

    wait(blocks_after_disable_witness, logs, minority_api_node)

    tt.logger.info(f'Reconnect sub networks')
    connect_sub_networks(sub_networks)
    
    sleep_seconds = 20
    tt.logger.info(f'Before sleep {sleep_seconds}')
    sleep(sleep_seconds)
    tt.logger.info(f'After sleep {sleep_seconds}')

    tt.logger.info(f'Enable {len(witness_details_part)} witnesses')
    enable_witnesses(majority_witness_wallet, witness_details_part)

    wait(blocks_after_enable_witness, logs, majority_api_node)
