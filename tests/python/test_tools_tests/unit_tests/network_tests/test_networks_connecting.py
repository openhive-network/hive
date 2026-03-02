from __future__ import annotations

from typing import TYPE_CHECKING

import pytest
from test_tools.__private.network import Network
from test_tools.__private.user_handles.get_implementation import get_implementation

if TYPE_CHECKING:
    from collections.abc import Iterable

    from test_tools.__private.user_handles import NetworkHandle


@pytest.mark.requires_hived_executables
def test_connecting_two_networks(two_networks_connected: Iterable[NetworkHandle]) -> None:
    # ARRANGE AND ACT is done in two_networks_connected fixture
    networks: list[Network] = [get_implementation(network, Network) for network in two_networks_connected]

    # ASSERT
    assert networks[0].connected_networks == {networks[1]}
    assert networks[1].connected_networks == {networks[0]}


@pytest.mark.requires_hived_executables
@pytest.mark.parametrize(
    "broadcast", [True, False], ids=("first_connecting_to_third_only", "second_connecting_to_third_also")
)
def test_connecting_three_networks(broadcast: bool, three_networks_connected: Iterable[NetworkHandle]) -> None:
    # ARRANGE AND ACT is partly done in two_networks_connected fixture
    networks: list[Network] = [get_implementation(network, Network) for network in three_networks_connected]

    # ACT
    # we don't have to connect second network with third network, because first network will broadcast the connections
    if not broadcast:
        networks[1].connect_with(networks[2])

    # ASSERT
    assert networks[0].connected_networks == {networks[i] for i in [1, 2]}
    assert networks[1].connected_networks == {networks[i] for i in [0, 2]}
    assert networks[2].connected_networks == {networks[i] for i in [0, 1]}


@pytest.mark.requires_hived_executables
def test_connecting_four_networks(four_networks_connected: Iterable[NetworkHandle]) -> None:
    # ARRANGE AND ACT is partly done in four_networks_connected fixture
    networks: list[Network] = [get_implementation(network, Network) for network in four_networks_connected]

    # ASSERT
    assert networks[0].connected_networks == {networks[i] for i in [1, 2, 3]}
    assert networks[1].connected_networks == {networks[i] for i in [0, 2, 3]}
    assert networks[2].connected_networks == {networks[i] for i in [0, 1, 3]}
    assert networks[3].connected_networks == {networks[i] for i in [0, 1, 2]}
