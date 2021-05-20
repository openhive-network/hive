from pathlib import Path
from shutil import rmtree

from .children_names import ChildrenNames
from .node import Node
from .wallet import Wallet
from . import logger


class Network:
    def __init__(self, name):
        self.name = name
        self.directory = Path('.').absolute()
        self.children_names = ChildrenNames()
        self.nodes = []
        self.__wallets = []
        self.is_running = False
        self.disconnected_networks = []
        self.logger = logger.getLogger(f'{__name__}.{self}')

        self.hived_executable_file_path = None
        self.wallet_executable_file_path = None

    def __str__(self):
        return self.name

    def set_directory(self, directory):
        self.directory = Path(directory).absolute()

    def set_hived_executable_file_path(self, path):
        self.hived_executable_file_path = Path(path).absolute()

    def set_wallet_executable_file_path(self, path):
        self.wallet_executable_file_path = Path(path).absolute()

    def get_directory(self):
        return self.directory / self.name

    def add_node(self, node_name):
        self.logger.warning(
            f'Method {self.add_node.__name__} is deprecated and will be removed in future version. '
            f'Use {self.create_node.__name__} instead.'
        )
        return self.create_node(node_name)

    def create_node(self, node_name=None):
        return self.__create_node(node_name, configure_for_block_production=False)

    def create_init_node(self, node_name='InitNode'):
        """Creates node which is ready to produce blocks"""
        return self.__create_node(node_name, configure_for_block_production=True)

    def __create_node(self, node_name, configure_for_block_production):
        if node_name is not None:
            self.children_names.register_name(node_name)
        else:
            node_name = self.children_names.create_name('Node')

        node = Node(self, node_name, configure_for_block_production=configure_for_block_production)
        self.nodes.append(node)
        return node

    def connect_nodes(self):
        if len(self.nodes) < 2:
            return

        from .port import Port
        seed_node = self.nodes[0]
        if not seed_node.config.p2p_endpoint:
            seed_node.config.p2p_endpoint = f'0.0.0.0:{Port.allocate()}'

        for node in self.nodes[1:]:
            node.add_seed_node(seed_node)

    def run(self, wait_for_live=True):
        directory = self.get_directory()
        if directory.exists():
            rmtree(directory)

        directory.mkdir(parents=True)

        self.connect_nodes()
        for node in self.nodes:
            node.set_directory(self.get_directory())
            node.set_executable_file_path(self.hived_executable_file_path)
            node.run(wait_for_live=False)
            node.wait_for_p2p_plugin_start()

        self.is_running = True

        if wait_for_live:
            self.wait_for_live_on_all_nodes()

    def close(self):
        for node in self.nodes:
            if node.is_running():
                node.close()

        for wallet in self.__wallets:
            if wallet.is_running():
                wallet.close()

    def attach_wallet_to(self, node):
        name = self.children_names.create_name(f'{node.name}Wallet')

        wallet = Wallet(name, self, self.get_directory())
        wallet.connect_to(node)
        wallet.run()

        self.__wallets.append(wallet)
        return wallet

    def connect_with(self, network):
        if len(self.nodes) == 0 or len(network.nodes) == 0:
            raise Exception('Unable to connect empty network')

        if not self.is_running:
            if any([node.is_able_to_produce_blocks() for node in self.nodes]):
                network.nodes[0].add_seed_node(self.nodes[0])
            else:
                self.nodes[0].add_seed_node(network.nodes[0])
            return

        if network not in self.disconnected_networks:
            raise Exception('Unsupported (yet): cannot connect networks when were already run')

        # Temporary implementation working only with one network
        self.allow_for_connections_with_anyone()
        self.disconnected_networks.remove(network)

        network.allow_for_connections_with_anyone()
        network.disconnected_networks.remove(self)

    def disconnect_from(self, network):
        if len(self.nodes) == 0 or len(network.nodes) == 0:
            raise Exception('Unable to disconnect empty network')

        self.disconnected_networks.append(network)
        network.disconnected_networks.append(self)

        self.allow_for_connections_only_between_nodes_in_network()
        network.allow_for_connections_only_between_nodes_in_network()

    def allow_for_connections_only_between_nodes_in_network(self):
        for node_number in range(len(self.nodes)):
            node = self.nodes[node_number]
            node.set_allowed_nodes(self.nodes[:node_number] + self.nodes[node_number+1:])

    def allow_for_connections_with_anyone(self):
        for node in self.nodes:
            node.set_allowed_nodes([])

    def wait_for_live_on_all_nodes(self):
        for node in self.nodes:
            node.wait_for_live()
