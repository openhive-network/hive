"""
***Example***

config = {
    "networks": [
                    {
                        "InitNode"     : { "Active": True, "Prepare": True },
                        "ApiNode"      : True,
                        "WitnessNodes" :[ 1, 2, { "Witnesses": 4, "Prepare": False } ]
                    },
                    {
                        "InitNode"     : False,
                        "ApiNode"      : { "Active": True, "Prepare": False },
                        "WitnessNodes" :[ { "Witnesses": 6, "Prepare": True }, 5 ]
                    }
                ]
}

na = NetworksArchitecture()
na.load(config)
print(na.show())

***Result***

[Network-alpha]
 (InitNode(P))
 (ApiNode())
 (WitnessNode-0()) (witness0-alpha)
 (WitnessNode-1()) (witness1-alpha, witness2-alpha)
 (WitnessNode-2()) (witness3-alpha, witness4-alpha, witness5-alpha, witness6-alpha)
[Network-beta]
 (ApiNode())
 (WitnessNode-0(P)) (witness7-beta, witness8-beta, witness9-beta, witness10-beta, witness11-beta, witness12-beta)
 (WitnessNode-1()) (witness13-beta, witness14-beta, witness15-beta, witness16-beta, witness17-beta)
"""

from typing import Callable, Tuple, Any

from dataclasses import dataclass, field

import test_tools as tt

@dataclass
class NodeWrapper:
    name    : str
    prepare : bool
    def show(self, offset: str = "") -> str:
        return f"{offset}({self.name}({'P' if self.prepare else ''}))"

@dataclass
class InitNodeWrapper(NodeWrapper):
    def show(self) -> str:
        return NodeWrapper.show(self, " ")

@dataclass
class ApiWrapper(NodeWrapper):
    def show(self) -> str:
        return NodeWrapper.show(self, " ")

@dataclass
class FullApiWrapper(NodeWrapper):
    def show(self) -> str:
        return NodeWrapper.show(self, " ")

@dataclass
class WitnessWrapper(NodeWrapper):
    witnesses : list = field(default_factory = list)

    def create_witnesses(self, cnt_witness_start: int, witnesses_number: int, network_number: int, processor: Callable[[str],int], legacy_witness_name: bool) -> None:
        for i in range(cnt_witness_start, witnesses_number):
            if legacy_witness_name:
                self.witnesses.append( f"witness-{network_number}-{i - cnt_witness_start}" ) #"witness-0-0", "witness-0-1", "witness-0-2"
            else:
                self.witnesses.append( f"witness{i}-{processor(network_number)}" ) #"witness0-alpha", "witness1-alpha", "witness2-alpha"

    def show(self) -> str:
        details = []

        for witness in self.witnesses:
            details.append(witness)

        return NodeWrapper.show(self, " ") + " (" + ", ".join( detail for detail in details) + ")"

class NetworkWrapper:
    def __init__(self, name) -> None:
        self.name           = name

        self.init_node: InitNodeWrapper | None      = None
        self.api_node: ApiWrapper | None            = None
        self.full_api_node: FullApiWrapper | None   = None
        self.witness_nodes: list[WitnessWrapper]    = []

    def show(self) -> str:
        details = []

        if self.init_node is not None:
            details.append(self.init_node.show())

        if self.api_node is not None:
            details.append(self.api_node.show())

        if self.full_api_node is not None:
            details.append(self.full_api_node.show())

        for witness_node in self.witness_nodes:
            details.append(witness_node.show())

        return f"[{self.name}]" + "\n" + "\n".join( detail for detail in details)

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

    def get_key_information(self, key_name: str, current_dict: dict, default: Any = False) -> Any:
        return current_dict.get(key_name, default)

    def get_api_init(self, node_name: str, current_dict: dict) -> Tuple[bool, bool]:
        #returns statuses: [active, prepare]
        node = self.get_key_information(node_name, current_dict)
        if isinstance(node, dict):
            return self.get_key_information("Active", node), self.get_key_information("Prepare", node)
        else:
            return node, False

    def get_witness(self, data: Any) -> Tuple[int, bool]:
        #returns statuses: [witnesses' number, prepare]
        if isinstance(data, dict):
            return self.get_key_information("Witnesses", data, -1), self.get_key_information("Prepare", data, -1)
        else:
            return data, False

    def load(self, config: dict) -> None:
        assert "networks" in config
        _networks = config["networks"]

        cnt_witness_start   = 0
        for cnt_network, network in enumerate(_networks):
            current_net = NetworkWrapper(f"Network-{self.greek(cnt_network)}")

            init_node_active, init_node_prepare = self.get_api_init("InitNode", network)
            if init_node_active:
                current_net.init_node = InitNodeWrapper("InitNode", init_node_prepare)
                self.nodes_number += 1

            api_node_active, api_node_prepare = self.get_api_init("ApiNode", network)
            if api_node_active:
                current_net.api_node = ApiWrapper("ApiNode", api_node_prepare)
                self.nodes_number += 1

            full_api_node_active, full_api_node_prepare = self.get_api_init("FullApiNode", network)
            if full_api_node_active:
                current_net.full_api_node = FullApiWrapper("FullApiNode", full_api_node_prepare)
                self.nodes_number += 1

            if witness_nodes := network.get("WitnessNodes", False):
                self.nodes_number += len(witness_nodes)

                for cnt_witness_node, witness_object in enumerate(witness_nodes):
                    witness_node_number, witness_node_prepare = self.get_witness(witness_object)

                    assert witness_node_number >= 0, "Expected number of witnesses >= 0"

                    current_witness = WitnessWrapper(f"WitnessNode-{cnt_witness_node}", witness_node_prepare)
                    current_witness.create_witnesses(cnt_witness_start, cnt_witness_start + witness_node_number, cnt_network, self.greek, self.legacy_witness_name)
                    cnt_witness_start += witness_node_number

                    current_net.witness_nodes.append(current_witness)

            self.networks.append(current_net)

    def show(self) -> str:
        details = []
        for network in self.networks:
            details.append(network.show())
        return "\n".join( detail for detail in details)

class NetworksBuilder:
    def __init__(self) -> None:
        self.init_node: tt.InitNode | None      = None
        self.witness_names: list[str]           = []
        self.networks: list[tt.Network]         = []
        self.nodes: list[tt.AnyNode]            = []
        self.prepare_nodes: list[tt.AnyNode]    = []
        self.init_wallet: tt.Wallet | None      = None

    def build(self, architecture: NetworksArchitecture, init_node_can_be_empty: bool = False) -> None:
        for network in architecture.networks:
            tt_network = tt.Network()

            if network.init_node is not None:
                assert self.init_node is None, "InitNode already exists"
                self.init_node = tt.InitNode(network=tt_network)
                self.nodes.append(self.init_node)
                if network.init_node.prepare:
                    self.prepare_nodes.append(self.init_node)

            for witness in network.witness_nodes:
                self.witness_names.extend(witness.witnesses)
                witness_node = tt.WitnessNode(witnesses=witness.witnesses, network=tt_network)
                self.nodes.append(witness_node)
                if witness.prepare:
                    self.prepare_nodes.append(witness_node)

            if network.api_node is not None:
                api_node = tt.ApiNode(network=tt_network)
                self.nodes.append(api_node)
                if network.api_node.prepare:
                    self.prepare_nodes.append(api_node)

            if network.full_api_node is not None:
                full_api_node = tt.FullApiNode(network=tt_network)
                self.nodes.append(full_api_node)
                if network.full_api_node.prepare:
                    self.prepare_nodes.append(full_api_node)

            self.networks.append(tt_network)
        assert init_node_can_be_empty or self.init_node is not None
