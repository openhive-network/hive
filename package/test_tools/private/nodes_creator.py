from pathlib import Path
import warnings

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

    def create_witness_node(self, name='WitnessNode', *, witnesses=None):
        node = self.__create_node(name, configure_for_block_production=False)
        assert 'witness' in node.config.plugin

        if witnesses is None:
            warnings.warn(
                f'You are creating witness node without setting witnesses. Probably you forget to define them like:\n'
                f'\n'
                f'  {self.create_witness_node.__name__}(witnesses=[\'witness0\', \'witness1\'])\n'
                f'\n'
                f'If you really want to create witness node without witnesses, then create node with explicit empty\n'
                f'list as argument, like this:\n'
                f'\n'
                f'  {self.create_witness_node.__name__}(witnesses=[])'
            )
            witnesses = []

        for witness in witnesses:
            node._register_witness(witness)

        return node

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
