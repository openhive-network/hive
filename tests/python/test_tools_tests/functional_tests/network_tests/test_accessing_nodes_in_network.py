from __future__ import annotations

import test_tools as tt


def test_getting_nodes_by_names_from_network() -> None:
    network = tt.Network()
    init_node = tt.InitNode(network=network)
    witness_node = tt.WitnessNode(network=network)
    api_nodes = [tt.ApiNode(network=network) for _ in range(2)]

    assert init_node is network.node("InitNode0")
    assert witness_node is network.node("WitnessNode0")
    assert api_nodes[0] is network.node("ApiNode0")
    assert api_nodes[1] is network.node("ApiNode1")
