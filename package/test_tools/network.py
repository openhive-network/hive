from pathlib import Path
from shutil import rmtree

from test_tools import constants
from test_tools.private.logger.logger_internal_interface import logger
from test_tools.private.nodes_creator import NodesCreator


class Network(NodesCreator):
    def __init__(self, name, directory):
        super().__init__()

        self.name = name
        self._directory = Path(directory).joinpath(self.name).absolute()
        self.__wallets = []
        self.network_to_connect_with = None
        self.disconnected_networks = []
        self.__clean_up_policy: constants.NetworkCleanUpPolicy = None
        self.logger = logger.create_child_logger(str(self))

    def __str__(self):
        return self.name

    def run(self, wait_for_live=True):
        if self._directory.exists():
            rmtree(self._directory)

        self._directory.mkdir(parents=True)

        if self.network_to_connect_with is None:
            seed_node = self._nodes[0]
            seed_node.run(wait_for_live=wait_for_live)
            nodes_connecting_to_seed = self._nodes[1:]
        else:
            seed_node = self.network_to_connect_with.nodes()[0]
            self.network_to_connect_with = None
            nodes_connecting_to_seed = self._nodes

        endpoint = seed_node.get_p2p_endpoint()

        for node in nodes_connecting_to_seed:
            node.config.p2p_seed_node.append(endpoint)
            node.run(wait_for_live=wait_for_live)

    def handle_final_cleanup(self, *, default_policy: constants.NetworkCleanUpPolicy):
        policy = default_policy if self.__clean_up_policy is None else self.__clean_up_policy

        corresponding_nodes_creator_policy = {
            constants.NetworkCleanUpPolicy.REMOVE_EVERYTHING:          super().CleanUpPolicy.REMOVE_EVERYTHING,
            constants.NetworkCleanUpPolicy.REMOVE_ONLY_UNNEEDED_FILES: super().CleanUpPolicy.REMOVE_ONLY_UNNEEDED_FILES,
            constants.NetworkCleanUpPolicy.DO_NOT_REMOVE_FILES:        super().CleanUpPolicy.DO_NOT_REMOVE_FILES,
        }

        self._handle_final_cleanup(default_policy=corresponding_nodes_creator_policy[policy])

        for wallet in self.__wallets:
            if wallet.is_running():
                wallet.close()

    def connect_with(self, network):
        if len(self._nodes) == 0 or len(network.nodes()) == 0:
            raise Exception('Unable to connect empty network')

        if not any(node.is_running() for node in self._nodes):
            if any(node.is_able_to_produce_blocks() for node in self._nodes):
                network.network_to_connect_with = self
            else:
                self.network_to_connect_with = network
            return

        if network not in self.disconnected_networks:
            raise Exception('Unsupported (yet): cannot connect networks when were already run')

        # Temporary implementation working only with one network
        self.allow_for_connections_with_anyone()
        self.disconnected_networks.remove(network)

        network.allow_for_connections_with_anyone()
        network.disconnected_networks.remove(self)

    def disconnect_from(self, network):
        if len(self._nodes) == 0 or len(network.nodes()) == 0:
            raise Exception('Unable to disconnect empty network')

        self.disconnected_networks.append(network)
        network.disconnected_networks.append(self)

        self.allow_for_connections_only_between_nodes_in_network()
        network.allow_for_connections_only_between_nodes_in_network()

    def allow_for_connections_only_between_nodes_in_network(self):
        for node_number in range(len(self._nodes)):
            node = self._nodes[node_number]
            node.set_allowed_nodes(self._nodes[:node_number] + self._nodes[node_number+1:])

    def allow_for_connections_with_anyone(self):
        for node in self._nodes:
            node.set_allowed_nodes([])

    def set_clean_up_policy(self, policy: constants.NetworkCleanUpPolicy):
        self.__clean_up_policy = policy
