from . import Network, Node
from .children_names import ChildrenNames


class World:
    def __init__(self):
        self.children_names = ChildrenNames()
        self.__networks = []
        self.__nodes = []

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
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
        self.__networks.append(network)
        return network

    def networks(self):
        return self.__networks

    def create_node(self, name=None):
        if name is not None:
            self.children_names.register_name(name)
        else:
            name = self.children_names.create_name('Node')

        node = Node(name)
        self.__nodes.append(node)
        return node

    def nodes(self):
        """Returns list of all nodes in the world (including nodes in networks)"""

        nodes = self.__nodes.copy()
        for network in self.__networks:
            nodes += network.nodes
        return nodes
