from pathlib import Path
import subprocess
import time


from .node_api.node_apis import Apis
from .account import Account
from . import logger


class Node:
    def __init__(self, name='unnamed', network=None, directory=None):
        self.api = Apis(self)

        self.network = network
        self.name = name
        self.directory = Path(directory) if directory is not None else Path(f'./{self.name}')
        self.print_to_terminal = False
        self.executable_file_path = None
        self.process = None
        self.stdout_file = None
        self.stderr_file = None
        self.finalizer = None
        self.logger = logger.getLogger(f'{__name__}.{self.network}.{self.name}')

        from .node_configs.default import create_default_config
        self.config = create_default_config()

    def __str__(self):
        return f'{self.network.name}::{self.name}' if self.network is not None else self.name

    @staticmethod
    def __close_process(process):
        if not process:
            return

        process.kill()
        process.wait()

    def set_directory(self, directory):
        self.directory = Path(directory).absolute()

    def is_running(self):
        if not self.process:
            return False

        return self.process.poll() is None

    def get_name(self):
        return self.name

    def get_p2p_endpoint(self):
        if not self.config.p2p_endpoint:
            self.config.p2p_endpoint = f'0.0.0.0:{self.network.allocate_port()}'

        return self.config.p2p_endpoint

    def add_seed_node(self, seed_node):
        endpoint = seed_node.get_p2p_endpoint()

        if endpoint is None:
            raise Exception(f'Cannot connect {self} to {seed_node}; has no endpoints')

        port = endpoint.split(':')[1]

        self.config.p2p_seed_node += [f'127.0.0.1:{port}']

    def redirect_output_to_terminal(self):
        self.print_to_terminal = True

    def is_http_listening(self):
        with open(self.directory / 'stderr.txt') as output:
            for line in output:
                if 'start listening for http requests' in line:
                    return True

        return False

    def is_ws_listening(self):
        # TODO: This can be implemented in smarter way...
        #       Fix also Node.is_http_listening
        #       and also Node.is_synced
        with open(self.directory/'stderr.txt') as output:
            for line in output:
                if 'start listening for ws requests' in line:
                    return True

        return False

    def wait_for_block(self, num):
        while True:
            with open(self.directory/'stderr.txt') as output:
                for line in output:
                    if f'transactions on block {num}' in line or f'Generated block #{num}' in line:
                        return
            time.sleep(1)

    def wait_for_synchronization(self):
        while not self.is_synchronized():
            time.sleep(1)

    def is_synchronized(self):
        with open(self.directory/'stderr.txt') as output:
            for line in output:
                if 'transactions on block' in line or 'Generated block #' in line:
                    return True

        return False

    def send(self, method, params=None, jsonrpc='2.0', id=1):
        message = {
            'jsonrpc': jsonrpc,
            'id': id,
            'method': method,
        }

        if params is not None:
            message['params'] = params

        if not self.config.webserver_http_endpoint:
            raise Exception('Webserver http endpoint is unknown')

        from urllib.parse import urlparse
        endpoint = f'http://{urlparse(self.config.webserver_http_endpoint, "http").path}'

        if '0.0.0.0' in endpoint:
            endpoint = endpoint.replace('0.0.0.0', '127.0.0.1')

        while not self.is_http_listening():
            time.sleep(1)

        from . import communication
        return communication.request(endpoint, message)

    def get_id(self):
        response = self.api.network_node.get_info()
        return response['result']['node_id']

    def set_allowed_nodes(self, nodes):
        return self.api.network_node.set_allowed_peers([node.get_id() for node in nodes])

    def run(self):
        if not self.executable_file_path:
            from .paths_to_executables import get_hived_path
            self.executable_file_path = get_hived_path()

        if self.directory.exists():
            from shutil import rmtree
            rmtree(self.directory)
        self.directory.mkdir(parents=True)

        config_file_path = self.directory.joinpath('config.ini')
        if self.config is not None:
            self.config.write_to_file(config_file_path)

        if not self.print_to_terminal:
            self.stdout_file = open(self.directory/'stdout.txt', 'w')
            self.stderr_file = open(self.directory/'stderr.txt', 'w')

        self.process = subprocess.Popen(
            [
                str(self.executable_file_path),
                '--chain-id=04e8b5fc4bb4ab3c0ee3584199a2e584bfb2f141222b3a0d1c74e8a75ec8ff39',
                '-d', '.'
            ],
            cwd=self.directory,
            stdout=None if self.print_to_terminal else self.stdout_file,
            stderr=None if self.print_to_terminal else self.stderr_file,
        )

        import weakref
        self.finalizer = weakref.finalize(self, Node.__close_process, self.process)

        if self.config is None:
            # Wait for config generation
            from time import sleep
            sleep(0.1)

            from .node_config import NodeConfig
            self.config = NodeConfig()
            self.config.load_from_file(self.directory / 'config.ini')

        print(f'[{self}] Run with pid {self.process.pid}, ', end='')
        if self.config.webserver_http_endpoint:
            print(f'with http server {self.config.webserver_http_endpoint}')
        else:
            print('without http server')

    def close(self):
        self.finalizer()

    def set_executable_file_path(self, executable_file_path):
        self.executable_file_path = executable_file_path

    def set_witness(self, witness_name, key=None):
        if 'witness' not in self.config.plugin:
            self.config.plugin += ['witness']

        if key is None:
            witness = Account(witness_name)
            key = witness.private_key

        self.config.witness += [witness_name]
        self.config.private_key += [key]
