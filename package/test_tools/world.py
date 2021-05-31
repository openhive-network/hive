from pathlib import Path

from .network import Network
from .node import Node
from .private.children_names import ChildrenNames
from .private.nodes_creator import NodesCreator


class World(NodesCreator):
    def __init__(self):
        super().__init__()

        self._children_names = ChildrenNames()
        self.__networks = []
        self.__wallets = []
        self.__name = 'World'
        self._directory = Path() / f'GeneratedIn{self}'

    def __str__(self):
        return self.__name

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        for wallet in self.__wallets:
            if wallet.is_running():
                wallet.close()

        for node in self._nodes:
            if node.is_running():
                node.close()

        for network in self.__networks:
            if network.is_running:
                network.close()

    def create_network(self, name=None):
        if name is not None:
            self._children_names.register_name(name)
        else:
            name = self._children_names.create_name('Network')

        network = Network(name)
        network.set_directory(self._directory)
        self.__networks.append(network)
        return network

    def networks(self):
        return self.__networks

    def attach_wallet_to(self, node, timeout):
        name = self._children_names.create_name(f'{node}Wallet')

        from .wallet import Wallet
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
