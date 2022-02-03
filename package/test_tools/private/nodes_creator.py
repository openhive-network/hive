from enum import auto, Enum
from pathlib import Path
import warnings

from test_tools import constants
from test_tools.account import Account
from test_tools.private.children_names import ChildrenNames
from test_tools.private.node import Node


class NodesCreator:
    class CleanUpPolicy(Enum):
        REMOVE_EVERYTHING = auto()
        REMOVE_ONLY_UNNEEDED_FILES = auto()
        DO_NOT_REMOVE_FILES = auto()

    def __init__(self):
        self._children_names = ChildrenNames()
        self._directory: Path = None  # Should be overriden by derived class
        self._nodes = []

    def create_init_node(self, name='InitNode') -> Node:
        """Creates node which is ready to produce blocks"""
        node = self.create_witness_node(name, witnesses=['initminer'])
        node.config.enable_stale_production = True
        node.config.required_participation = 0
        return node

    def create_witness_node(self, name=None, *, witnesses=None) -> Node:
        node = self.__create_node_preconfigured_for_tests(name, default_name='WitnessNode')
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
            self.__register_witness(node, witness)

        return node

    def create_api_node(self, name=None) -> Node:
        node = self.__create_node_preconfigured_for_tests(name, default_name='ApiNode')
        node.config.plugin.remove('witness')
        return node

    def __create_node_preconfigured_for_tests(self, name, *, default_name) -> Node:
        node = self.create_raw_node(name, default_name=default_name)
        self.__enable_all_api_plugins(node)
        node.config.shared_file_size = '128M'
        return node

    def create_raw_node(self, name=None, *, default_name='RawNode') -> Node:
        if name is not None:
            self._children_names.register_name(name)
        else:
            name = self._children_names.create_name(default_name)

        node = Node(self, name, self._directory)
        self._nodes.append(node)
        return node

    def node(self, name: str) -> Node:
        for node in self._nodes:
            if node.get_name() == name:
                return node

        raise RuntimeError(
            f'There is no node with name "{name}". Available nodes are:\n'
            f'{[node.get_name() for node in self._nodes]}'
        )

    @staticmethod
    def __register_witness(node, witness_name):
        witness = Account(witness_name)
        node.config.witness.append(witness.name)
        node.config.private_key.append(witness.private_key)

    @staticmethod
    def __enable_all_api_plugins(node):
        node.config.plugin.append('account_history_rocksdb')  # Required by account_history_api

        all_api_plugins = [plugin for plugin in node.get_supported_plugins() if plugin.endswith('_api')]
        node.config.plugin.extend([plugin for plugin in all_api_plugins if plugin not in node.config.plugin])

    def nodes(self):
        return self._nodes

    def _handle_final_cleanup(self, *, default_policy: CleanUpPolicy):
        for node in self._nodes:
            node.handle_final_cleanup(default_policy=self.__get_corresponding_node_policy(default_policy))

    @staticmethod
    def __get_corresponding_node_policy(policy: CleanUpPolicy):
        NodePolicy = constants.NodeCleanUpPolicy
        NodesCreatorPolicy = NodesCreator.CleanUpPolicy

        return {
            NodesCreatorPolicy.REMOVE_EVERYTHING:          NodePolicy.REMOVE_EVERYTHING,
            NodesCreatorPolicy.REMOVE_ONLY_UNNEEDED_FILES: NodePolicy.REMOVE_ONLY_UNNEEDED_FILES,
            NodesCreatorPolicy.DO_NOT_REMOVE_FILES:        NodePolicy.DO_NOT_REMOVE_FILES,
        }[policy]
