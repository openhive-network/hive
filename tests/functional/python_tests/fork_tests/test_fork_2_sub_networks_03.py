from time import sleep

import shared_tools.complex_networks_helper_functions as sh
import test_tools as tt

def test_fork_2_sub_networks_03(prepare_fork_2_sub_networks_03):
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
    witness_details_part  = sh.get_part_of_witness_details(witness_details, 4, 16)

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

    logs.append(sh.NodeLog("M", majority_witness_wallet))
    logs.append(sh.NodeLog("m", tt.Wallet(attach_to = minority_api_node)))

    _M = logs[0].collector
    _m = logs[1].collector

    blocks_before_disconnect        = 10
    blocks_after_disconnect         = 2
    blocks_after_disable_witness    = 10

    tt.logger.info(f'Before disconnecting')
    cnt = 0
    while True:
        sh.wait(1, logs, majority_api_node)

        cnt += 1
        if cnt > blocks_before_disconnect:
            if sh.get_last_irreversible_block_num(_M) == sh.get_last_irreversible_block_num(_m):
                break

    tt.logger.info(f'Disconnect sub networks')
    sh.disconnect_sub_networks(sub_networks)

    sh.wait(blocks_after_disconnect, logs, majority_api_node)

    tt.logger.info(f'Disable {len(witness_details_part)} witnesses')
    tt.logger.info(f'{witness_details_part}')
    sh.disable_witnesses(majority_witness_wallet, witness_details_part)
    tt.logger.info(f'Witnesses are disabled')

    sh.wait(blocks_after_disable_witness, logs, minority_api_node)

    tt.logger.info(f'Reconnect sub networks')
    sh.connect_sub_networks(sub_networks)

    sleep_seconds = 20
    tt.logger.info(f'Before sleep {sleep_seconds}')
    sleep(sleep_seconds)
    tt.logger.info(f'After sleep {sleep_seconds}')

    tt.logger.info(f'Enable {len(witness_details_part)} witnesses')
    sh.enable_witnesses(majority_witness_wallet, witness_details_part)


    sh.wait_for_final_block(minority_api_node, logs, [_m, _M], True, sh.lib_true_condition, False)

    sh.assert_no_duplicates(minority_api_node, majority_api_node)
