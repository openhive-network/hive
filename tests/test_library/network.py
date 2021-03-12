from pathlib import Path
from shutil import rmtree

from node import Node
from wallet import Wallet


class Network:
    def __init__(self, name):
        self.name = name
        self.directory = Path('.').absolute()
        self.nodes = []
        self.next_free_port = 49152

        self.hived_executable_file_path = None
        self.wallet_executable_file_path = None

    def allocate_port(self):
        port = self.next_free_port
        self.next_free_port += 1
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
        node = Node(name=node_name)
        self.nodes.append(node)
        return node

    def assign_ports_for_nodes(self):
        for node in self.nodes:
            node.add_p2p_endpoint(f'0.0.0.0:{self.allocate_port()}')
            node.add_webserver_http_endpoint(f'0.0.0.0:{self.allocate_port()}')
            node.add_webserver_ws_endpoint(f'0.0.0.0:{self.allocate_port()}')

    def connect_nodes(self):
        if len(self.nodes) < 2:
            return

        seed_node = self.nodes[0]
        for node in self.nodes[1:]:
            node.add_seed_node(seed_node)

    def run(self):
        print('Script is run in', Path('.').absolute())

        if not self.hived_executable_file_path:
            raise Exception('Missing hived executable, use Network.set_hived_executable_file_path')

        directory = self.get_directory()
        if directory.exists():
            print('Clear whole directory', directory)
            rmtree(directory)

        directory.mkdir(parents=True)

        self.assign_ports_for_nodes()
        self.connect_nodes()

        for node in self.nodes:
            node.set_directory(self.get_directory() / node.get_name())
            node.set_executable_file_path(self.hived_executable_file_path)
            node.run()

    def attach_wallet(self):
        if len(self.nodes) == 0:
            raise Exception('Cannot connect wallet to network without nodes')

        return self.attach_wallet_to(self.nodes[0])

    def attach_wallet_to(self, node):
        if len(self.nodes) == 0:
            raise Exception('Cannot connect wallet to network without nodes')

        wallet = Wallet()
        wallet.set_http_server_port(self.allocate_port())
        wallet.connect_to(node)

        wallet.set_executable_file_path(self.wallet_executable_file_path)
        wallet.run()

        return wallet
