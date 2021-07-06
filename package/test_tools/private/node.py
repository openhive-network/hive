import math
import json
from pathlib import Path
import shutil
import subprocess
import time

from test_tools import Account, logger, paths_to_executables
from test_tools.exceptions import CommunicationError, NodeIsNotRunning
from test_tools.node_api.node_apis import Apis
from test_tools.wallet import Wallet


class Node:
    __DEFAULT_WAIT_FOR_LIVE_TIMEOUT = 20

    def __init__(self, creator, name, directory):
        self.api = Apis(self)

        import weakref
        self.__creator = weakref.proxy(creator)
        self.__name = name
        self.directory = Path(directory).joinpath(self.__name).absolute()
        self.__produced_files = False
        self.__executable_file_path = None
        self.__process = None
        self.__stdout_file = None
        self.__stderr_file = None
        self.__logger = logger.getLogger(f'{self.__creator}.{self.__name}')

        from test_tools.node_configs.default import create_default_config
        self.config = create_default_config()

    def __str__(self):
        from test_tools.network import Network
        return f'{self.__creator}::{self.__name}' if isinstance(self.__creator, Network) else self.__name

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
        result = subprocess.check_output([str(self.__get_path_of_executable()), '--chain-id'], stderr=subprocess.STDOUT)
        return result.decode('utf-8').strip()

    def __get_path_of_executable(self):
        default_path = paths_to_executables.get_path_of('hived')
        return default_path if self.__executable_file_path is None else self.__executable_file_path

    def is_running(self):
        if not self.__process:
            return False

        return self.__process.poll() is None

    def is_able_to_produce_blocks(self):
        conditions = [
            self.config.enable_stale_production,
            self.config.required_participation == 0,
            bool(self.config.witness),
            bool(self.config.private_key),
        ]

        return all(conditions)

    def attach_wallet(self, timeout=15) -> Wallet:
        if not self.is_running():
            raise NodeIsNotRunning('Before attaching wallet you have to run node')

        return self.__creator.attach_wallet_to(self, timeout)

    def get_name(self):
        return self.__name

    def add_seed_node(self, seed_node):
        if seed_node.config.p2p_endpoint is None:
            from test_tools.port import Port
            seed_node.config.p2p_endpoint = f'0.0.0.0:{Port.allocate()}'

        endpoint = seed_node.config.p2p_endpoint
        port = endpoint.split(':')[1]

        self.config.p2p_seed_node.append(f'127.0.0.1:{port}')

    def __is_p2p_plugin_started(self):
        return self.__any_line_in_stderr(lambda line: 'P2P Plugin started' in line)

    def __is_http_listening(self):
        return self.__any_line_in_stderr(lambda line: 'start listening for http requests' in line)

    def _is_ws_listening(self):
        return self.__any_line_in_stderr(lambda line: 'start listening for ws requests' in line)

    def __is_live(self):
        return self.__any_line_in_stderr(
            lambda line: 'transactions on block' in line or 'Generated block #' in line
        )

    def __any_line_in_stderr(self, predicate):
        with open(self.directory / 'stderr.txt') as output:
            for line in output:
                if predicate(line):
                    return True

        return False

    def wait_number_of_blocks(self, blocks_to_wait, *, timeout=math.inf):
        assert blocks_to_wait > 0
        self.wait_for_block_with_number(self.__get_last_block_number() + blocks_to_wait, timeout=timeout)

    def wait_for_block_with_number(self, number, *, timeout=math.inf):
        from test_tools.private.wait_for import wait_for
        wait_for(
            lambda: self.__is_block_with_number_reached(number),
            timeout=timeout,
            timeout_error_message=f'Waiting too long for block {number}',
            poll_time=2.0
        )

    def __is_block_with_number_reached(self, number):
        last = self.__get_last_block_number()
        return last >= number

    def __get_last_block_number(self):
        response = self.api.database.get_dynamic_global_properties()
        return response['result']['head_block_number']

    def _wait_for_p2p_plugin_start(self, timeout=10):
        from test_tools.private.wait_for import wait_for
        wait_for(self.__is_p2p_plugin_started, timeout=timeout,
                 timeout_error_message=f'Waiting too long for start of {self} p2p plugin')

    def _wait_for_live(self, timeout=__DEFAULT_WAIT_FOR_LIVE_TIMEOUT):
        from test_tools.private.wait_for import wait_for
        wait_for(self.__is_live, timeout=timeout,
                 timeout_error_message=f'Waiting too long for {self} live (to start produce or receive blocks)')

    def _send(self, method, params=None, jsonrpc='2.0', id=1):
        message = {
            'jsonrpc': jsonrpc,
            'id': id,
            'method': method,
        }

        if params is not None:
            message['params'] = params

        if self.config.webserver_http_endpoint is None:
            raise Exception('Webserver http endpoint is unknown')

        from urllib.parse import urlparse
        endpoint = f'http://{urlparse(self.config.webserver_http_endpoint, "http").path}'

        if '0.0.0.0' in endpoint:
            endpoint = endpoint.replace('0.0.0.0', '127.0.0.1')

        self.__wait_for_http_listening()

        from test_tools import communication
        response = communication.request(endpoint, message)

        if 'error' in response:
            raise CommunicationError(f'Error detected in response from node {self}', message, response)

        return response

    def __wait_for_http_listening(self, timeout=10):
        from test_tools.private.wait_for import wait_for
        wait_for(self.__is_http_listening, timeout=timeout,
                 timeout_error_message=f'Waiting too long for {self} to start listening on http port')

    def get_id(self):
        response = self.api.network_node.get_info()
        return response['result']['node_id']

    def set_allowed_nodes(self, nodes):
        return self.api.network_node.set_allowed_peers(allowed_peers=[node.get_id() for node in nodes])

    def run(self, wait_for_live=True, timeout=__DEFAULT_WAIT_FOR_LIVE_TIMEOUT, use_existing_config=False):
        """
        :param wait_for_live: Stops execution until node will generate or receive blocks.
        :param timeout: If wait_for_live is set to True, this parameter sets how long waiting can take. When
                        timeout is reached, TimeoutError exception is thrown.
        :param use_existing_config: Skip generation of config file and use already existing. It means that all
                                    current config values will be ignored and overridden by values from file.
                                    When config file is missing hived generates default config.
        """
        if not self.__is_test_net_build():
            from test_tools import paths_to_executables
            raise NotImplementedError(
                f'You have configured path to non-testnet hived build.\n'
                f'At the moment only testnet build is supported.\n'
                f'Your current hived path is: {paths_to_executables.get_path_of("hived")}\n'
                f'\n'
                f'Please check following page if you need help with paths configuration:\n'
                f'https://gitlab.syncad.com/hive/test-tools/-/blob/master/documentation/paths_to_executables.md'
            )

        if not self.__produced_files and self.directory.exists():
            from shutil import rmtree
            rmtree(self.directory)
        self.directory.mkdir(parents=True, exist_ok=True)

        config_file_path = self.directory.joinpath('config.ini')
        if not use_existing_config:
            self.__set_unset_endpoints()
            self.config.write_to_file(config_file_path)

        self.__stdout_file = open(self.directory/'stdout.txt', 'w')
        self.__stderr_file = open(self.directory/'stderr.txt', 'w')

        self.__process = subprocess.Popen(
            [str(self.__get_path_of_executable()), '-d', '.'],
            cwd=self.directory,
            stdout=self.__stdout_file,
            stderr=self.__stderr_file,
        )

        if use_existing_config:
            # Wait for config generation
            while not config_file_path.exists():
                time.sleep(0.01)

            from test_tools.node_config import NodeConfig
            self.config = NodeConfig()
            self.config.load_from_file(config_file_path)

        self.__produced_files = True
        if wait_for_live:
            self._wait_for_live(timeout)

        self.__log_run_summary()

    def __log_run_summary(self):
        message = f'Run with pid {self.__process.pid}, '

        endpoints = self.__get_opened_endpoints()
        if endpoints:
            message += f'with servers: {", ".join([f"{endpoint[1]}://{endpoint[0]}" for endpoint in endpoints])}'
        else:
            message += 'without any server'

        message += f', {self.__get_executable_build_version()} build'
        message += f' commit={self.__get_commit_hash_of_executable()[:8]}'
        self.__logger.info(message)

    def __get_opened_endpoints(self):
        endpoints = [
            (self.config.webserver_http_endpoint, 'http'),
            (self.config.webserver_ws_endpoint, 'ws'),
            (self.config.webserver_unix_endpoint, 'unix'),
        ]

        return [endpoint for endpoint in endpoints if endpoint[0] is not None]

    def __set_unset_endpoints(self):
        from test_tools.port import Port

        if self.config.p2p_endpoint is None:
            self.config.p2p_endpoint = f'0.0.0.0:{Port.allocate()}'

        if self.config.webserver_http_endpoint is None:
            self.config.webserver_http_endpoint = f'0.0.0.0:{Port.allocate()}'

        if self.config.webserver_ws_endpoint is None:
            self.config.webserver_ws_endpoint = f'0.0.0.0:{Port.allocate()}'

    def __get_commit_hash_of_executable(self):
        output = subprocess.check_output([str(self.__get_path_of_executable()), '--version'], stderr=subprocess.STDOUT)
        output = json.loads(f'{{{output.decode("utf-8")}}}')
        return output['version']['hive_git_revision']

    def close(self, remove_unneeded_files=True):
        self.__close_process()
        self.__close_opened_files()

        if remove_unneeded_files:
            self.__remove_unneeded_files()

    def restart(self, wait_for_live=True, timeout=__DEFAULT_WAIT_FOR_LIVE_TIMEOUT):
        self.close(remove_unneeded_files=False)
        self.run(wait_for_live=wait_for_live, timeout=timeout)

    def __close_process(self):
        if self.__process is None:
            return

        import signal
        self.__process.send_signal(signal.SIGINT)
        try:
            return_code = self.__process.wait(timeout=10)
            self.__logger.debug(f'Closed with {return_code} return code')
        except subprocess.TimeoutExpired:
            self.__process.kill()
            self.__process.wait()
            self.__logger.warning(f'Process was force-closed with SIGKILL, because didn\'t close before timeout')

    def __close_opened_files(self):
        for file in [self.__stdout_file, self.__stderr_file]:
            file.close()

    def __remove_unneeded_files(self):
        unneeded_files_or_directories = [
            'blockchain/',
            'p2p/',
        ]

        def remove(path: Path):
            try:
                shutil.rmtree(path) if path.is_dir() else path.unlink()
            except FileNotFoundError:
                pass  # It is ok that file to remove was removed earlier or never existed

        for unneeded in unneeded_files_or_directories:
            remove(self.directory.joinpath(unneeded))

    def set_executable_file_path(self, executable_file_path):
        self.__executable_file_path = executable_file_path
