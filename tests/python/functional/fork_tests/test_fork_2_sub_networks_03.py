from __future__ import annotations

import pytest

import shared_tools.complex_networks_helper_functions as sh
import test_tools as tt


@pytest.mark.fork_tests_group_1()
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

    networks_builder = prepare_fork_2_sub_networks_03

    witness_details_part = sh.get_part_of_witness_details(networks_builder.witness_names, 4, 16)

    minority_api_node = networks_builder.networks[0].node("FullApiNode0")
    majority_api_node = networks_builder.networks[1].node("FullApiNode1")
    majority_witness_node = networks_builder.networks[1].node("WitnessNode1")

    majority_witness_wallet = tt.Wallet(attach_to=majority_witness_node)
    set_expiration_time = 1000
    majority_witness_wallet.api.set_transaction_expiration(set_expiration_time)

    initminer_private_key = "5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n"
    majority_witness_wallet.api.import_key(initminer_private_key)

    logs = []

    logs.extend((sh.NodeLog("M", majority_witness_wallet), sh.NodeLog("m", tt.Wallet(attach_to=minority_api_node))))

    _M = logs[0].collector
    _m = logs[1].collector

    blocks_before_disconnect = 10
    blocks_after_disconnect = 2
    blocks_after_disable_witness = 10

    tt.logger.info("Before disconnecting")
    cnt = 0
    while True:
        sh.wait(1, logs, majority_api_node)

        cnt += 1
        if cnt > blocks_before_disconnect:  # noqa: SIM102
            if sh.get_last_irreversible_block_num(_M) == sh.get_last_irreversible_block_num(_m):
                break

    tt.logger.info("Disconnect sub networks")
    sh.disconnect_sub_networks(networks_builder.networks)

    sh.wait(blocks_after_disconnect, logs, majority_api_node)

    tt.logger.info(f"Disable {len(witness_details_part)} witnesses")
    tt.logger.info(f"{witness_details_part}")
    sh.disable_witnesses(majority_witness_wallet, witness_details_part)
    tt.logger.info("Witnesses are disabled")

    sh.wait(blocks_after_disable_witness, logs, minority_api_node)

    tt.logger.info("Reconnect sub networks")
    sh.connect_sub_networks(networks_builder.networks)

    sleep_seconds = 20
    tt.logger.info(f"Wait {sleep_seconds} seconds")
    for _cnt in range(sleep_seconds):
        sh.wait(1, logs, minority_api_node, False)
    tt.logger.info(f"{sleep_seconds} seconds passed")

    tt.logger.info(f"Enable {len(witness_details_part)} witnesses")
    sh.enable_witnesses(majority_witness_wallet, witness_details_part)

    sh.wait_for_final_block(minority_api_node, logs, [_m, _M], True, sh.lib_true_condition, False)

    sh.assert_no_duplicates(minority_api_node, majority_api_node)
