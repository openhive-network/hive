import math
from pathlib import Path
import subprocess
import time
import weakref


from .node_api.node_apis import Apis
from .account import Account
from . import logger
from .wallet import Wallet


class NodeIsNotRunning(Exception):
    pass


class Node:
    def __init__(self, creator, name, directory=None, configure_for_block_production=False):
        self.api = Apis(self)

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

        if configure_for_block_production:
            self.config.enable_stale_production = True
            self.config.required_participation = 0
            self.config.witness.append('initminer')

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

    def __get_executable_build_version(self):
        if self.__is_test_net_build():
            return 'testnet'
        elif self.__is_main_net_build():
            return 'mainnet'
        else:
            return 'unrecognised'

    def __is_test_net_build(self):
        error_message = self.__run_hived_with_chain_id_argument()
        return error_message == 'Error parsing command line: the required argument for option \'--chain-id\' is missing'

    def __is_main_net_build(self):
        error_message = self.__run_hived_with_chain_id_argument()
        return error_message == 'Error parsing command line: unrecognised option \'--chain-id\''

    def __run_hived_with_chain_id_argument(self):
        hived_path = self.executable_file_path
        if hived_path is None:
            from test_tools import paths_to_executables
            hived_path = paths_to_executables.get_path_of('hived')

        result = subprocess.check_output([str(hived_path), '--chain-id'], stderr=subprocess.STDOUT)
        return result.decode('utf-8').strip()

    def set_directory(self, directory):
        self.directory = Path(directory).absolute() / self.name

    def is_running(self):
        if not self.process:
            return False

        return self.process.poll() is None

    def is_able_to_produce_blocks(self):
        conditions = [
            self.config.enable_stale_production,
            self.config.required_participation == 0,
            bool(self.config.witness),
            bool(self.config.private_key),
        ]

        return all(conditions)

    def attach_wallet(self) -> Wallet:
        if not self.is_running():
            raise NodeIsNotRunning('Before attaching wallet you have to run node')

        return self.creator.attach_wallet_to(self)

    def get_name(self):
        return self.name

    def add_seed_node(self, seed_node):
        if not seed_node.config.p2p_endpoint.is_set():
            from .port import Port
            seed_node.config.p2p_endpoint = f'0.0.0.0:{Port.allocate()}'

        endpoint = seed_node.config.p2p_endpoint
        port = endpoint.split(':')[1]

        self.config.p2p_seed_node.append(f'127.0.0.1:{port}')

    def redirect_output_to_terminal(self):
        self.print_to_terminal = True

    def is_p2p_plugin_started(self):
        return self.__any_line_in_stderr(lambda line: 'P2P Plugin started' in line)

    def is_http_listening(self):
        return self.__any_line_in_stderr(lambda line: 'start listening for http requests' in line)

    def is_ws_listening(self):
        return self.__any_line_in_stderr(lambda line: 'start listening for ws requests' in line)

    def is_synchronized(self):
        return self.__any_line_in_stderr(
            lambda line: 'transactions on block' in line or 'Generated block #' in line
        )

    def __any_line_in_stderr(self, predicate):
        with open(self.directory / 'stderr.txt') as output:
            for line in output:
                if predicate(line):
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

    def wait_for_p2p_plugin_start(self):
        while not self.is_p2p_plugin_started():
            self.logger.debug('Waiting for p2p plugin start...')
            time.sleep(1)

    def wait_for_synchronization(self, timeout=math.inf):
        poll_time = 1.0
        while not self.is_synchronized():
            if timeout <= 0:
                raise TimeoutError('Timeout of node synchronization was reached')

            self.logger.debug('Waiting for synchronization...')
            time.sleep(min(poll_time, timeout))
            timeout -= poll_time

    def send(self, method, params=None, jsonrpc='2.0', id=1):
        message = {
            'jsonrpc': jsonrpc,
            'id': id,
            'method': method,
        }

        if params is not None:
            message['params'] = params

        if not self.config.webserver_http_endpoint.is_set():
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
        return self.api.network_node.set_allowed_peers(allowed_peers=[node.get_id() for node in nodes])

    def run(self, wait_for_live=True, timeout=math.inf, use_existing_config=False):
        """
        :param wait_for_live: Stops execution until node will generate or receive blocks.
        :param timeout: If wait_for_live is set to True, this parameter sets how long waiting can take. When
                        timeout is reached, TimeoutError exception is thrown.
        :param use_existing_config: Skip generation of config file and use already existing. It means that all
                                    current config values will be ignored and overridden by values from file.
                                    When config file is missing hived generates default config.
        """
        if not self.executable_file_path:
            from . import paths_to_executables
            self.executable_file_path = paths_to_executables.get_path_of('hived')

        if not self.__is_test_net_build():
            from test_tools import paths_to_executables
            raise NotImplementedError(
                f'You have configured path to non-testnet hived build.\n'
                f'At the moment only testnet build is supported.\n'
                f'Your current hived path is: {paths_to_executables.get_path_of("hived")}\n'
                f'\n'
                f'Please check following page if you need help with paths configuration:\n'
                f'https://gitlab.syncad.com/hive/test-tools/-/blob/develop/documentation/paths_to_executables.md'
            )

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

        self.finalizer = weakref.finalize(self, Node.__close_process, self.process, self.logger)

        if use_existing_config:
            # Wait for config generation
            while not config_file_path.exists():
                time.sleep(0.01)

            from .node_config import NodeConfig
            self.config = NodeConfig()
            self.config.load_from_file(config_file_path)

        self.produced_files = True
        if wait_for_live:
            self.wait_for_synchronization(timeout)

        message = f'Run with pid {self.process.pid}, '
        if self.config.webserver_http_endpoint:
            message += f'with http server {self.config.webserver_http_endpoint}'
        else:
            message += 'without http server'
        message += f', {self.__get_executable_build_version()} build'
        self.logger.info(message)

    def __set_unset_endpoints(self):
        from .port import Port

        if not self.config.p2p_endpoint.is_set():
            self.config.p2p_endpoint = f'0.0.0.0:{Port.allocate()}'

        if not self.config.webserver_http_endpoint.is_set():
            self.config.webserver_http_endpoint = f'0.0.0.0:{Port.allocate()}'

        if not self.config.webserver_ws_endpoint.is_set():
            self.config.webserver_ws_endpoint = f'0.0.0.0:{Port.allocate()}'

    def close(self):
        self.finalizer()

    def set_executable_file_path(self, executable_file_path):
        self.executable_file_path = executable_file_path
