from __future__ import annotations

import pytest

import test_tools as tt

from .block_log.generate_block_log import AMOUNT_OF_ALL_COMMENTS


@pytest.mark.parametrize("network_type", ["bastion", "complete_graph"])
def test_cumulative_payout_for_comments(prepare_environment, network_type) -> None:
    networks_builder = prepare_environment
    api_node = networks_builder.networks[0].node("FullApiNode0")
    cashout_time = tt.Time.parse(
        api_node.api.database.get_comment_pending_payouts(comments=[["creator-0", "post-creator-0"]])["cashout_infos"][0]["cashout_info"]["cashout_time"]
    )
    tt.logger.info(f"Cashout time: {cashout_time}")

    if network_type == "bastion":
        build_fortress(networks_builder)

    ht = api_node.get_head_block_time()
    tt.logger.info(f"Head block time: {ht}")
    last_block_number = api_node.get_last_block_number()
    tt.logger.info(f"Head block number: {last_block_number}")

    error_message = (
        f"Cashout_time: {cashout_time} is already in the past. Actual head_block time: {ht} should be before"
        " cashout_time."
    )
    assert api_node.get_head_block_time() < cashout_time, error_message

    wait_for_comment_payout(api_node, cashout_time)

    wait_for_irreversible_head_block(api_node)

    err_msg = "Not all comment rewards have been paid out."
    assert (
        len(
            api_node.api.account_history.enum_virtual_ops(
                block_range_begin=last_block_number,
                block_range_end=last_block_number + 1000,
                include_reversible=True,
                filter=0x000800,
            )["ops"]
        )
        == AMOUNT_OF_ALL_COMMENTS
    ), err_msg


def build_fortress(network) -> None:
    """
    The function isolates witness nodes, ensuring that each mining node (castle) has its own dedicated API node (bastion).
    While the bastions are interconnected, the castles remain disconnected from one another.
    """
    castle_nodes = [node for node in network.nodes if isinstance(node, (tt.InitNode, tt.WitnessNode))]
    bastion_nodes = [node for node in network.nodes if isinstance(node, (tt.ApiNode, tt.FullApiNode))]

    error_message = "The number of Init/Witness Nodes (miner-nodes) does not correspond to the number of ApiNodes"
    assert len(castle_nodes) == len(bastion_nodes), error_message

    for castle, bastion in zip(castle_nodes, bastion_nodes):
        castle.api.network_node.set_allowed_peers(allowed_peers=[get_id(bastion)])
        bastion_nodes_ids = [get_id(node) for node in bastion_nodes if get_id(node) != get_id(bastion)]
        bastion.api.network_node.set_allowed_peers(allowed_peers=[*bastion_nodes_ids, get_id(castle)])


def get_id(node) -> str:
    response = node.api.network_node.get_info()
    return response["node_id"]


def wait_for_comment_payout(node, cashout_time) -> None:
    def is_comment_payout() -> bool:
        return bool(node.get_head_block_time() > cashout_time)

    tt.logger.info("Wait for cashout...")
    tt.Time.wait_for(is_comment_payout)
    tt.logger.info("Cashout done.")


def wait_for_irreversible_head_block(node) -> None:
    """
    Function waiting for network stabilization. Quits when forking is complete.
    """

    def check_if_head_block_is_last_irreversible() -> bool:
        return node.get_last_block_number() == node.get_last_irreversible_block_number()

    tt.Time.wait_for(
        check_if_head_block_is_last_irreversible,
        timeout=tt.Time.seconds(120),
        timeout_error_message="The time limit for establishing a common fork has been exceeded.",
    )
    tt.logger.info(f"Forking finished at block: {node.get_last_irreversible_block_number()}")
