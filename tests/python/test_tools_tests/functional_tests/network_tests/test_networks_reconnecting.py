from __future__ import annotations

from typing import TYPE_CHECKING

import test_tools as tt
from local_tools.network import three_networks_connected, two_networks_connected  # noqa: F401

if TYPE_CHECKING:
    from collections.abc import Iterable


def _log_head_block_numbers(networks: Iterable[tt.Network]) -> None:
    for network in networks:
        tt.logger.info(f"Head block number of {network} is {network.get_last_block_number()}")


def test_reconnecting_2_networks(two_networks_connected: Iterable[tt.Network]) -> None:
    # ARRANGE
    first_network, second_network = two_networks_connected

    for network in two_networks_connected:
        network.run()

    # ACT
    second_network_head_num_before_disconnect = second_network.get_last_block_number()

    tt.logger.info("Disconnecting first_network from second_network")
    first_network.disconnect_from(second_network)
    first_network.node("InitNode0").wait_number_of_blocks(1)

    tt.logger.info("Connecting first_network with second_network")
    first_network.connect_with(second_network)

    # ASSERT
    second_network.node("ApiNode0").wait_for_block_with_number(
        second_network_head_num_before_disconnect + 1, timeout=60
    )

    _log_head_block_numbers(two_networks_connected)


def test_reconnecting_3_networks(three_networks_connected: Iterable[tt.Network]) -> None:
    # ARRANGE
    first_network, second_network, third_network = three_networks_connected

    for network in three_networks_connected:
        network.run()

    # ACT
    second_network_head_num_before_disconnect = second_network.get_last_block_number()
    third_network_head_num_before_disconnect = third_network.get_last_block_number()

    tt.logger.info("Disconnecting first_network from all networks")
    first_network.disconnect_from_all()
    first_network.node("InitNode0").wait_number_of_blocks(1)

    tt.logger.info("Connecting first_network with second_network")
    first_network.connect_with(second_network)
    tt.logger.info("Connecting first_network with third_network")
    first_network.connect_with(third_network)

    # ASSERT
    second_network.node("ApiNode0").wait_for_block_with_number(
        second_network_head_num_before_disconnect + 1, timeout=60
    )
    third_network.node("ApiNode1").wait_for_block_with_number(third_network_head_num_before_disconnect + 1, timeout=60)

    _log_head_block_numbers(three_networks_connected)
