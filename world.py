from pathlib import Path

from . import Network, Node
from .children_names import ChildrenNames


class World:
    def __init__(self):
        self.children_names = ChildrenNames()
        self.__networks = []
        self.__nodes = []
        self.__wallets = []
        self.__directory = Path() / 'GeneratedInWorld'

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        for wallet in self.__wallets:
            if wallet.is_running():
                wallet.close()

        for node in self.__nodes:
            if node.is_running():
                node.close()

        for network in self.__networks:
            if network.is_running:
                network.close()

    def create_network(self, name=None):
        if name is not None:
            self.children_names.register_name(name)
        else:
            name = self.children_names.create_name('Network')

        network = Network(name)
        network.set_directory(self.__directory)
        self.__networks.append(network)
        return network

    def networks(self):
        return self.__networks

    def create_node(self, name=None):
        return self.__create_node(name, configure_for_block_production=False)

    def create_init_node(self, name='InitNode'):
        """Creates node which is ready to produce blocks"""
        return self.__create_node(name, configure_for_block_production=True)

    def __create_node(self, name, configure_for_block_production):
        if name is not None:
            self.children_names.register_name(name)
        else:
            name = self.children_names.create_name('Node')

        node = Node(self, name, configure_for_block_production=configure_for_block_production)
        node.set_directory(self.__directory)
        self.__nodes.append(node)
        return node

    def attach_wallet_to(self, node):
        name = self.children_names.create_name(f'{node}Wallet')

        from .wallet import Wallet
        wallet = Wallet(self, node.directory.parent / name)
        wallet.connect_to(node)
        wallet.run()

        self.__wallets.append(wallet)
        return wallet

    def nodes(self):
        """Returns list of all nodes in the world (including nodes in networks)"""

        nodes = self.__nodes.copy()
        for network in self.__networks:
            nodes += network.nodes
        return nodes
