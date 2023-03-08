import shared_tools.complex_networks_helper_functions as sh
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

    networks_builder   = prepare_fork_2_sub_networks_02

    witness_details_part  = sh.get_part_of_witness_details(networks_builder.witness_names, 6, 14)

    minority_api_node       = networks_builder.networks[0].node('ApiNode0')
    majority_api_node       = networks_builder.networks[1].node('ApiNode1')
    majority_witness_node   = networks_builder.networks[1].node('WitnessNode1')

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

    blocks_after_disable_witness    = 15

    blocks_after_disconnect         = 20

    blocks_after_enable_witness     = 20

    blocks_after_reconnect          = 5

    tt.logger.info(f'Before disconnecting')
    cnt = 0
    while True:
        sh.wait(1, logs, majority_api_node)

        cnt += 1
        if cnt > blocks_before_disconnect:
            if sh.get_last_irreversible_block_num(_M) == sh.get_last_irreversible_block_num(_m):
                break

    tt.logger.info(f'Disable {len(witness_details_part)} witnesses')
    tt.logger.info(f'{witness_details_part}')
    sh.disable_witnesses(majority_witness_wallet, witness_details_part)

    sh.wait(blocks_after_disable_witness, logs, minority_api_node)

    last_head_m = sh.get_last_head_block_number(_m)
    last_head_M = sh.get_last_head_block_number(_M)

    last_lib_m  = sh.get_last_irreversible_block_num(_m)
    last_lib_M  = sh.get_last_irreversible_block_num(_M)

    tt.logger.info(f'Disconnect sub networks')
    sh.disconnect_sub_networks(networks_builder.networks)

    sh.wait(blocks_after_disconnect, logs, minority_api_node)

    tt.logger.info(f'Reconnect sub networks')
    sh.connect_sub_networks(networks_builder.networks)

    sh.wait(blocks_after_reconnect, logs, minority_api_node)

    tt.logger.info(f'Enable {len(witness_details_part)} witnesses')
    sh.enable_witnesses(majority_witness_wallet, witness_details_part)
    sh.wait(blocks_after_enable_witness, logs, minority_api_node)

    assert last_head_m < sh.get_last_head_block_number(_m)
    assert last_head_M < sh.get_last_head_block_number(_M)

    assert last_lib_m  < sh.get_last_irreversible_block_num(_m)
    assert last_lib_M  < sh.get_last_irreversible_block_num(_M)

    sh.wait_for_final_block(minority_api_node, logs, [_m, _M], True, sh.lib_true_condition, False)

    sh.assert_no_duplicates(minority_api_node, majority_api_node)
