from pathlib import Path
import warnings

from test_tools import Account
from test_tools.private.children_names import ChildrenNames
from test_tools.private.node import Node


class NodesCreator:
    def __init__(self):
        self._children_names = ChildrenNames()
        self._directory: Path = None  # Should be overriden by derived class
        self._nodes = []

    def create_node(self, name=None):
        warnings.warn(
            f'Method {self.create_node.__name__} is deprecated. Use one of following methods instead:\n'
            f'- {self.create_init_node.__name__}\n'
            f'- {self.create_witness_node.__name__}\n'
            f'- {self.create_api_node.__name__}\n'
            f'- {self.create_raw_node.__name__}\n'
            f'\n'
            f'Read following documentation page if you don\'t know which one you need:\n'
            f'https://gitlab.syncad.com/hive/test-tools/-/blob/master/documentation/node_types.md'
        )

        return self.create_raw_node(name)

    def create_init_node(self, name='InitNode'):
        """Creates node which is ready to produce blocks"""
        node = self.create_witness_node(name, witnesses=['initminer'])
        node.config.enable_stale_production = True
        node.config.required_participation = 0
        return node

    def create_witness_node(self, name=None, *, witnesses=None):
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

    def create_api_node(self, name=None):
        node = self.__create_node_preconfigured_for_tests(name, default_name='ApiNode')
        node.config.plugin.remove('witness')
        return node

    def __create_node_preconfigured_for_tests(self, name, *, default_name):
        node = self.create_raw_node(name, default_name=default_name)
        self.__enable_all_api_plugins(node)
        node.config.shared_file_size = '128M'
        return node

    def create_raw_node(self, name=None, *, default_name='RawNode'):
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

        from test_tools.private.node_config_entry_types import Plugin
        all_api_plugins = [plugin for plugin in Plugin.SUPPORTED_PLUGINS if plugin.endswith('_api')]
        node.config.plugin.extend([plugin for plugin in all_api_plugins if plugin not in node.config.plugin])

    def nodes(self):
        return self._nodes.copy()
