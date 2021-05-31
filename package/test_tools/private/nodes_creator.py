from pathlib import Path

from test_tools.children_names import ChildrenNames
from test_tools.node import Node


class NodesCreator:
    def __init__(self):
        self._children_names: ChildrenNames = None  # Should be overriden by derived class
        self._directory: Path = None  # Should be overriden by derived class
        self._nodes = []

    def create_node(self, name=None):
        return self.__create_node(name, configure_for_block_production=False)

    def create_init_node(self, name='InitNode'):
        """Creates node which is ready to produce blocks"""
        return self.__create_node(name, configure_for_block_production=True)

    def __create_node(self, name, configure_for_block_production):
        if name is not None:
            self._children_names.register_name(name)
        else:
            name = self._children_names.create_name('Node')

        node = Node(self, name, configure_for_block_production=configure_for_block_production)
        node.set_directory(self._directory)
        self._nodes.append(node)
        return node

    def nodes(self):
        return self._nodes.copy()
