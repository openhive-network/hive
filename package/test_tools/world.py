from pathlib import Path

from test_tools import constants
from test_tools.network import Network
from test_tools.wallet import Wallet
from test_tools.private.children_names import ChildrenNames
from test_tools.private.node import Node
from test_tools.private.nodes_creator import NodesCreator


class World(NodesCreator):
    def __init__(self, directory=None):
        super().__init__()

        self.__networks = []
        self.__wallets = []
        self.__name = 'World'
        self.__is_monitoring_resources = False

        if directory is None:
            self._directory = Path() / f'GeneratedIn{self}'
        else:
            self._directory = directory

    def __str__(self):
        return self.__name

    def __enter__(self):
        if self.__is_monitoring_resources:
            raise RuntimeError('You already have activated context manager (`with World(): ...`)')

        self.__is_monitoring_resources = True
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        if self.__is_monitoring_resources:
            self.close()

    def close(self):
        self.handle_final_cleanup()

    def handle_final_cleanup(self):
        if not self.__is_monitoring_resources:
            raise RuntimeError('World was already closed. Can be closed only once.')

        super().handle_final_cleanup()

        for wallet in self.__wallets:
            if wallet.is_running():
                wallet.close()

        for network in self.__networks:
            network.handle_final_cleanup()

    def create_network(self, name=None):
        if name is not None:
            self._children_names.register_name(name)
        else:
            name = self._children_names.create_name('Network')

        network = Network(name, self._directory)
        self.__networks.append(network)
        return network

    def network(self, name: str) -> Network:
        for network in self.__networks:
            if network.get_name() == name:
                return network

        raise RuntimeError(
            f'There is no network with name "{name}". Available networks are:\n'
            f'{[node.get_name() for node in self._nodes]}'
        )

    def networks(self):
        return self.__networks

    def attach_wallet_to(self, node, timeout):
        name = self._children_names.create_name(f'{node}Wallet')

        wallet = Wallet(name, self, node.directory.parent)
        wallet.connect_to(node)
        wallet.run(timeout)

        self.__wallets.append(wallet)
        return wallet

    def nodes(self):
        """Returns list of all nodes in the world (including nodes in networks)"""

        nodes = self._nodes.copy()
        for network in self.__networks:
            nodes += network.nodes()
        return nodes

    def set_directory(self, directory):
        self._directory = Path(directory)
