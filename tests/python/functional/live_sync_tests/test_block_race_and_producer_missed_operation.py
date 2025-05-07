from __future__ import annotations

import test_tools as tt
import shared_tools.networks_architecture as networks


def test_block_race_and_producer_missed_operation(
    prepare_4_4_4_4_4_with_time_offset_and_full_api_node: networks.NetworksBuilder,
):
    """
    The test involves a network with 21 witnesses, out of which 4 have their time set to the current time plus 4 seconds.
    This causes these witnesses to produce their blocks too early, resulting in a "race" with witnesses who produce blocks
    according to the schedule. As a result, there are situations where an incorrect block enters the chain and is accepted
    by other witnesses. In this scenario, the block from the correct witness is lost, and the witness receives a virtual
    operation called "producer_missed_operation".
    """
    networks_builder = prepare_4_4_4_4_4_with_time_offset_and_full_api_node

    api_node = networks_builder.networks[1].node("FullApiNode0")

    def is_producer_missed_operation_appear() -> bool:
        vops = api_node.api.account_history.enum_virtual_ops(block_range_begin=0, block_range_end=1000)["ops"]
        operation_types = (vop["op"]["type"] for vop in vops)
        return "producer_missed_operation" in operation_types

    tt.Time.wait_for(
        is_producer_missed_operation_appear,
        timeout=3 * 70,
        timeout_error_message="producer_missed_operation NOT APPEAR",
    )
