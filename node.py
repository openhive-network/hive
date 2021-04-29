from pathlib import Path
import subprocess
import time


from .node_api.node_apis import Apis
from .account import Account
from . import logger


class Node:
    def __init__(self, creator, name='unnamed', directory=None):
        self.api = Apis(self)

        import weakref
        self.creator = weakref.proxy(creator)
        self.name = name
        self.directory = Path(directory) if directory is not None else Path(f'./{self.name}')
        self.produced_files = False
        self.print_to_terminal = False
        self.executable_file_path = None
        self.process = None
        self.stdout_file = None
        self.stderr_file = None
        self.finalizer = None
        self.logger = logger.getLogger(f'{__name__}.{self.creator}.{self.name}')

        from .node_configs.default import create_default_config
        self.config = create_default_config()

    def __str__(self):
        from .network import Network
        return f'{self.creator}::{self.name}' if isinstance(self.creator, Network) else self.name

    @staticmethod
    def __close_process(process, logger_from_node):
        if not process:
            return

        import signal
        process.send_signal(signal.SIGINT)
        try:
            return_code = process.wait(timeout=3)
            logger_from_node.debug(f'Closed with {return_code} return code')
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()
            logger_from_node.warning(f"Send SIGKILL because process didn't close before timeout")

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
            self.config.p2p_endpoint = f'0.0.0.0:{self.creator.allocate_port()}'

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

    def wait_number_of_blocks(self, blocks_to_wait):
        assert blocks_to_wait > 0
        self.wait_for_block_with_number(self.__get_last_block_number() + blocks_to_wait)

    def wait_for_block_with_number(self, number):
        last_printed = None

        last = self.__get_last_block_number()
        while last < number:
            if last_printed != last:
                self.logger.debug(f'Waiting for block with number {number} (last: {last})')
                last_printed = last

            time.sleep(2)
            last = self.__get_last_block_number()

    def __get_last_block_number(self):
        response = self.api.database.get_dynamic_global_properties()
        return response['result']['head_block_number']

    def wait_for_synchronization(self):
        while not self.is_synchronized():
            self.logger.debug('Waiting for synchronization...')
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

    def run(self, wait_until_live=True, use_existing_config=False):
        """
        :param wait_until_live: Stops execution until node will generate or receive blocks.
        :param use_existing_config: Skip generation of config file and use already existing. It means that all
                                    current config values will be ignored and overridden by values from file.
                                    When config file is missing hived generates default config.
        """
        if not self.executable_file_path:
            from . import paths_to_executables
            self.executable_file_path = paths_to_executables.get_path_of('hived')

        if not self.produced_files and self.directory.exists():
            from shutil import rmtree
            rmtree(self.directory)
        self.directory.mkdir(parents=True, exist_ok=True)

        config_file_path = self.directory.joinpath('config.ini')
        if not use_existing_config:
            self.__set_unset_endpoints()
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
        self.finalizer = weakref.finalize(self, Node.__close_process, self.process, self.logger)

        if use_existing_config:
            # Wait for config generation
            from time import sleep
            while not config_file_path.exists():
                sleep(0.01)

            from .node_config import NodeConfig
            self.config = NodeConfig()
            self.config.load_from_file(config_file_path)

        self.produced_files = True
        if wait_until_live:
            self.wait_for_synchronization()

        message = f'Run with pid {self.process.pid}, '
        if self.config.webserver_http_endpoint:
            message += f'with http server {self.config.webserver_http_endpoint}'
        else:
            message += 'without http server'
        self.logger.info(message)

    def __set_unset_endpoints(self):
        from .node_config import NodeConfig
        if self.config.p2p_endpoint is NodeConfig.UNSET:
            self.config.p2p_endpoint = f'0.0.0.0:{self.creator.allocate_port()}'

        if self.config.webserver_http_endpoint is NodeConfig.UNSET:
            self.config.webserver_http_endpoint = f'0.0.0.0:{self.creator.allocate_port()}'

        if self.config.webserver_ws_endpoint is NodeConfig.UNSET:
            self.config.webserver_ws_endpoint = f'0.0.0.0:{self.creator.allocate_port()}'

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
