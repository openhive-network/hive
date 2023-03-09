import json

from typing import List
from typing import Callable

from dataclasses import dataclass, field

import test_tools as tt

@dataclass
class NodeWrapper:
    name : str

@dataclass
class InitNodeWrapper(NodeWrapper): pass

@dataclass
class ApiWrapper(NodeWrapper): pass

class WitnessWrapper(NodeWrapper):
    def __init__(self, name) -> None:
        super().__init__(name)
        self.witnesses = []

    def create_witnesses(self, cnt_witness_start: int, witnesses_number: int, network_number: int, processor: Callable[[str],int], legacy_witness_name: bool) -> None:
        for i in range(cnt_witness_start, witnesses_number):
            if legacy_witness_name:
                self.witnesses.append( f"witness-{network_number}-{i - cnt_witness_start}" ) #"witness-0-0", "witness-0-1", "witness-0-2"
            else:
                self.witnesses.append( f"witness{i}-{processor(network_number)}" ) #"witness0-alpha", "witness1-alpha", "witness2-alpha"

    def __str__(self) -> str:
        return f"({self.name} {''.join( '(' + witness_name + ')' for witness_name in self.witnesses)})"

class NetworkWrapper:
    def __init__(self, name) -> None:
        self.name           = name

        self.init_node      = None
        self.api_node       = None
        self.witness_nodes  = []

    def __str__(self) -> str:
        details = []
        details.append(f"({self.name})")

        if self.init_node is not None:
            details.append(str(self.init_node))

        if self.api_node is not None:
            details.append(str(self.api_node))

        for witness_node in self.witness_nodes:
            details.append(str(witness_node))

        return "\n  ".join( detail for detail in details)

@dataclass
class NetworksArchitecture:
    legacy_witness_name : bool = False
    networks            : list = field(default_factory = list)
    nodes_number        : int = 0

    def greek(self, idx: int) -> str:
        greek_alphabet = ["alpha",      "beta",     "gamma",    "delta",
                          "epsi",       "zeta",     "eta",      "theta",
                          "iota",       "kappa",    "lambda",   "mu",
                          "nu",         "xi",       "omi",      "pi",
                          "rho",        "sigma",    "tau",      "upsi",
                          "phi",        "chi",      "psi",      "omega" ]
        return greek_alphabet[idx % len(greek_alphabet)]

    def load(self, config: dict) -> None:
        assert "networks" in config
        _networks = config["networks"]

        cnt_witness_start   = 0
        for cnt_network, network in enumerate(_networks):
            current_net = NetworkWrapper(f"Network-{self.greek(cnt_network)}")

            if network.get("InitNode", False):
                current_net.init_node = InitNodeWrapper("InitNode")
                self.nodes_number += 1

            if network.get("ApiNode", False):
                current_net.api_node = ApiWrapper("ApiNode")
                self.nodes_number += 1

            if witness_nodes := network.get("WitnessNodes", False):
                self.nodes_number += len(witness_nodes)
                for cnt_witness_node, witnesses_number in enumerate(witness_nodes):
                    current_witness = WitnessWrapper(f"WitnessNode-{cnt_witness_node}")
                    current_witness.create_witnesses(cnt_witness_start, cnt_witness_start + witnesses_number, cnt_network, self.greek, self.legacy_witness_name)
                    cnt_witness_start += witnesses_number

                    current_net.witness_nodes.append(current_witness)

            self.networks.append(current_net)

class NetworksBuilder:
    def __init__(self) -> None:
        self.init_node      = None
        self.witness_names  = []
        self.networks       = []
        self.nodes          = []
        self.init_wallet    = None

    def build(self, architecture: NetworksArchitecture) -> None:
        for network in architecture.networks:
            tt_network = tt.Network()

            if network.init_node is not None:
                assert self.init_node == None, "InitNode already exists"
                self.init_node = tt.InitNode(network=tt_network)
                self.nodes.append(self.init_node)

            for witness in network.witness_nodes:
                self.witness_names.extend(witness.witnesses)
                self.nodes.append(tt.WitnessNode(witnesses=witness.witnesses, network=tt_network))

            if network.api_node is not None:
                self.nodes.append(tt.ApiNode(network=tt_network))

            self.networks.append(tt_network)
        assert self.init_node is not None

#=================Example=================

# config = {
#     "networks": [
#                     {
#                         "InitNode"     : True,
#                         "ApiNode"      : True,
#                         "WitnessNodes" :[ 1, 2, 4 ]
#                     },
#                     {
#                         "InitNode"     : False,
#                         "ApiNode"      : True,
#                         "WitnessNodes" :[ 6, 5 ]
#                     }
#                 ]
# }

# na = NetworksArchitecture()
# na.load(config)
# print(na)

#=================Result=================

# (Network-alpha)
#   (InitNode)
#   (ApiNode)
#   (WitnessNode-0 (witness0-alpha))
#   (WitnessNode-1 (witness1-alpha)(witness2-alpha))
#   (WitnessNode-2 (witness3-alpha)(witness4-alpha)(witness5-alpha)(witness6-alpha))
# (Network-beta)
#   (ApiNode)
#   (WitnessNode-0 (witness7-beta)(witness8-beta)(witness9-beta)(witness10-beta)(witness11-beta)(witness12-beta))
#   (WitnessNode-1 (witness13-beta)(witness14-beta)(witness15-beta)(witness16-beta)(witness17-beta))
