from pathlib import Path
from shutil import rmtree

from .node import Node
from .wallet import Wallet
from . import logger


class Network:
    def __init__(self, name, port_range=range(49152, 65536)):
        self.name = name
        self.directory = Path('.').absolute()
        self.nodes = []
        self.port_range = port_range
        self.next_free_port = port_range.start
        self.is_running = False
        self.disconnected_networks = []
        self.logger = logger.getLogger(f'{__name__}.{self}')

        self.hived_executable_file_path = None
        self.wallet_executable_file_path = None

    def __str__(self):
        return self.name

    def allocate_port(self):
        if self.next_free_port not in self.port_range:
            raise Exception('There is no free ports to use')

        port = self.next_free_port
        self.next_free_port += self.port_range.step
        return port

    def set_directory(self, directory):
        self.directory = Path(directory).absolute()

    def set_hived_executable_file_path(self, path):
        self.hived_executable_file_path = Path(path).absolute()

    def set_wallet_executable_file_path(self, path):
        self.wallet_executable_file_path = Path(path).absolute()

    def get_directory(self):
        return self.directory / self.name

    def add_node(self, node_name):
        node = Node(name=node_name, network=self)
        self.nodes.append(node)
        return node

    def assign_ports_for_nodes(self):
        for node in self.nodes:
            if node.get_p2p_endpoint() is None:
                node.add_p2p_endpoint(f'0.0.0.0:{self.allocate_port()}')

            from .node_config import NodeConfig
            if node.config.webserver_http_endpoint is NodeConfig.UNSET:
                node.config.webserver_http_endpoint = f'0.0.0.0:{self.allocate_port()}'

            if node.config.webserver_ws_endpoint is NodeConfig.UNSET:
                node.config.webserver_ws_endpoint = f'0.0.0.0:{self.allocate_port()}'

    def connect_nodes(self):
        if len(self.nodes) < 2:
            return

        seed_node = self.nodes[0]
        for node in self.nodes[1:]:
            node.add_seed_node(seed_node)

    def run(self):
        directory = self.get_directory()
        if directory.exists():
            rmtree(directory)

        directory.mkdir(parents=True)

        self.assign_ports_for_nodes()
        self.connect_nodes()

        for node in self.nodes:
            node.set_directory(self.get_directory() / node.get_name())
            node.set_executable_file_path(self.hived_executable_file_path)
            node.run()

        self.is_running = True

    def attach_wallet(self):
        if len(self.nodes) == 0:
            raise Exception('Cannot connect wallet to network without nodes')

        return self.attach_wallet_to(self.nodes[0])

    def attach_wallet_to(self, node):
        if len(self.nodes) == 0:
            raise Exception('Cannot connect wallet to network without nodes')

        wallet = Wallet(self.get_directory() / 'Wallet')
        wallet.set_http_server_port(self.allocate_port())
        wallet.connect_to(node)

        wallet.set_executable_file_path(self.wallet_executable_file_path)
        wallet.run()

        return wallet

    def connect_with(self, network):
        if len(self.nodes) == 0 or len(network.nodes) == 0:
            raise Exception('Unable to connect empty network')

        if not self.is_running:
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

    def wait_for_synchronization_of_all_nodes(self):
        for node in self.nodes:
            node.wait_for_synchronization()
