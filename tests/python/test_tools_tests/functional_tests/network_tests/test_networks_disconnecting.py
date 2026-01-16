from __future__ import annotations

from enum import Enum
from typing import TYPE_CHECKING

import pytest
import test_tools as tt
from local_tools.network import (
    get_head_block_number,
    get_head_block_numbers_for_networks,
    three_networks_connected,
    two_networks_connected,
)  # noqa: F401

from wax.helpy import Hf26Asset as Asset
from wax.helpy import Time

if TYPE_CHECKING:
    from collections.abc import Iterable

    from test_tools.__private.type_annotations.any_node import AnyNode


class DisconnectionType(Enum):
    DISCONNECT_FROM = 1
    DISCONNECT_FROM_REVERSE = 2
    DISCONNECT_ALL = 3


def prepare_witness(node: AnyNode, account: tt.Account) -> int:
    """
    Create witness account.

    Waits until it is listed in the schedule and return head block number when it is certain
    that the witness is capable of producing blocks.
    """
    wallet = tt.Wallet(attach_to=node)

    wallet.api.create_account_with_keys(
        "initminer",
        account.name,
        "",
        account.public_key,
        account.public_key,
        account.public_key,
        account.public_key,
    )

    wallet.api.transfer_to_vesting("initminer", account.name, Asset.Test(1000))

    wallet.api.import_key(tt.Account("witness0").private_key)

    wallet.api.update_witness(
        account.name,
        "https://" + account.name,
        account.public_key,
        {"account_creation_fee": Asset.Test(3), "maximum_block_size": 65536, "hbd_interest_rate": 0},
    )

    # Witness schedule list is updated on a block which is a multiple of 21. After fast confirmation feature
    # we need to wait for witness schedule update twice to make sure that the witness is listed in the schedule.
    tt.logger.info(f"Waiting for witness {account.name} to be listed in the schedule")
    maximum_time_when_witness_should_be_capable_of_producing_blocks = 2 * 21 * 3
    Time.wait_for(
        lambda: account.name in node.api.database.get_witness_schedule()["current_shuffled_witnesses"],
        timeout=maximum_time_when_witness_should_be_capable_of_producing_blocks,
    )
    return get_head_block_number(node=node)


def disconnect_two_networks_in_specified_way(
    first_network: tt.Network, second_network: tt.Network, disconnection_type: DisconnectionType
) -> None:
    if disconnection_type == DisconnectionType.DISCONNECT_FROM:
        tt.logger.info("Disconnecting first_network from second_network")
        first_network.disconnect_from(second_network)
    elif disconnection_type == DisconnectionType.DISCONNECT_FROM_REVERSE:
        tt.logger.info("Disconnecting second_network from first_network")
        second_network.disconnect_from(first_network)
    elif disconnection_type == DisconnectionType.DISCONNECT_ALL:
        tt.logger.info("Disconnecting first_network from all")
        first_network.disconnect_from_all()
    else:
        raise ValueError(f"Unknown disconnection type: {disconnection_type}")


@pytest.mark.parametrize("disconnection_type", list(DisconnectionType))
def test_disconnecting_2_networks(
    disconnection_type: DisconnectionType, two_networks_connected: Iterable[tt.Network]
) -> None:
    # ARRANGE
    first_network, second_network = two_networks_connected

    for network in two_networks_connected:
        network.run()

    # ACT
    disconnect_two_networks_in_specified_way(first_network, second_network, disconnection_type)
    first_network.node("InitNode0").wait_number_of_blocks(1)

    # ASSERT
    head_block_numbers = get_head_block_numbers_for_networks(networks=two_networks_connected)

    assert head_block_numbers[first_network] != head_block_numbers[second_network]


@pytest.mark.parametrize(
    "disconnection_type", [DisconnectionType.DISCONNECT_FROM, DisconnectionType.DISCONNECT_FROM_REVERSE]
)
def test_disconnecting_1_of_3_networks(
    disconnection_type: DisconnectionType, three_networks_connected: Iterable[tt.Network]
) -> None:
    # ARRANGE
    first_network, second_network, _ = three_networks_connected

    for network in three_networks_connected:
        network.run()

    # ACT
    disconnect_two_networks_in_specified_way(first_network, second_network, disconnection_type)
    second_network_head_num_after_disconnect = get_head_block_number(network=second_network)

    first_network.node("InitNode0").wait_number_of_blocks(1)

    second_network.node("ApiNode0").wait_for_block_with_number(second_network_head_num_after_disconnect + 1, timeout=60)

    # ASSERT
    head_block_numbers = get_head_block_numbers_for_networks(three_networks_connected)

    assert head_block_numbers[second_network] > second_network_head_num_after_disconnect


@pytest.mark.parametrize("disconnection_type", [DisconnectionType.DISCONNECT_FROM, DisconnectionType.DISCONNECT_ALL])
def test_separating_1_active_from_3_networks(
    disconnection_type: DisconnectionType, three_networks_connected: Iterable[tt.Network]
) -> None:
    # ARRANGE
    first_network, second_network, third_network = three_networks_connected

    for network in three_networks_connected:
        network.run()

    # ACT
    if disconnection_type == DisconnectionType.DISCONNECT_FROM:
        tt.logger.info("Disconnecting first_network from second_network and third_network")
        first_network.disconnect_from(second_network)
        first_network.disconnect_from(third_network)
    elif disconnection_type == DisconnectionType.DISCONNECT_ALL:
        tt.logger.info("Disconnecting first_network from all networks")
        first_network.disconnect_from_all()

    first_network.node("InitNode0").wait_number_of_blocks(1)

    # ASSERT
    # Wait for second and third networks to synchronize before checking
    # (they may be at different blocks momentarily due to timing/CI load)
    import time

    max_sync_attempts = 10
    for _attempt in range(max_sync_attempts):
        head_block_numbers = get_head_block_numbers_for_networks(three_networks_connected)

        # Check if second and third are synchronized
        if head_block_numbers[second_network] == head_block_numbers[third_network]:
            break

        # Wait for block production to settle
        time.sleep(0.5)
    else:
        # Final read after retries
        head_block_numbers = get_head_block_numbers_for_networks(three_networks_connected)

    assert (
        head_block_numbers[first_network] > head_block_numbers[second_network]
    ), f"First network should be ahead: first={head_block_numbers[first_network]}, second={head_block_numbers[second_network]}"
    assert (
        head_block_numbers[second_network] == head_block_numbers[third_network]
    ), f"Second and third should be synchronized: second={head_block_numbers[second_network]}, third={head_block_numbers[third_network]}"


def test_separating_2_networks_producing_blocks_from_3_networks(three_networks_connected: Iterable[tt.Network]) -> None:
    # ARRANGE
    first_network, second_network, third_network = three_networks_connected

    first_network_init_node = first_network.node("InitNode0")

    witness_account = tt.Account("witness0")
    second_network_witness_node = tt.WitnessNode(witnesses=[witness_account.name], network=second_network)
    second_network_witness_node.config.enable_stale_production = True
    second_network_witness_node.config.required_participation = 0

    for network in three_networks_connected:
        network.run()

    prepare_witness(node=first_network_init_node, account=witness_account)

    # ACT
    tt.logger.info("Disconnecting first_network from all networks")
    first_network.disconnect_from_all()

    tt.logger.info("Disconnecting second_network from all networks")
    second_network.disconnect_from_all()

    first_network_init_node.wait_number_of_blocks(1)
    second_network_witness_node.wait_number_of_blocks(1)

    # ASSERT
    # both networks should generate block with this same block_number
    block_number_to_check = get_head_block_number(network=first_network)

    # both witnesses should generate block every 6 seconds
    first_network_init_node.wait_for_block_with_number(block_number_to_check, timeout=10)

    head_block_numbers = get_head_block_numbers_for_networks(three_networks_connected)

    assert head_block_numbers[first_network] > head_block_numbers[third_network]
    assert head_block_numbers[second_network] > head_block_numbers[third_network]

    assert (
        second_network_witness_node.api.block.get_block(block_num=block_number_to_check)["block"]["witness"]
        == witness_account.name
    )
    assert (
        first_network_init_node.api.block.get_block(block_num=block_number_to_check)["block"]["witness"] == "initminer"
    )
