from __future__ import annotations

import test_tools as tt


def test_network_startup() -> None:
    network = tt.Network()
    tt.InitNode(network=network)
    tt.WitnessNode(witnesses=[], network=network)
    tt.ApiNode(network=network)

    network.run()


def test_two_connected_networks_startup() -> None:
    first = tt.Network()
    tt.InitNode(network=first)
    for _ in range(3):
        tt.ApiNode(network=first)

    second = tt.Network()
    for _ in range(3):
        tt.ApiNode(network=second)

    first.connect_with(second)

    first.run()
    second.run()
