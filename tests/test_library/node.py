from pathlib import Path
import subprocess
import time


from .node_config import NodeConfig
from .witness import Witness


class Node:
    class __Apis:
        pass

    class __ApiBase:
        def __init__(self, node, name):
            self.__name = name
            self.__node = node

        def send(self, method, params=None, jsonrpc='2.0', id=1):
            return self.__node.send(f'{self.__name}.{method}', params, jsonrpc=jsonrpc, id=id)

    class __DatabaseApi(__ApiBase):
        def __init__(self, node):
            super().__init__(node, 'database_api')

    def __init__(self, name='unnamed', network=None, directory=Path()):
        self.api = Node.__Apis()
        self.api.database = Node.__DatabaseApi(self)

        self.network = network
        self.name = name
        self.directory = directory
        self.print_to_terminal = False
        self.executable_file_path = None
        self.process = None
        self.stdout_file = None
        self.stderr_file = None

        self.config = NodeConfig()
        self.config.add_entry(
            'log-appender',
            '{"appender":"stderr","stream":"std_error"} {"appender":"p2p","file":"logs/p2p/p2p.log"}',
            'Appender definition json: {"appender", "stream", "file"} Can only specify a file OR a stream'
        )

        self.config.add_entry(
            'log-logger',
            '{"name": "default", "level": "info", "appender": "stderr"} {"name": "p2p", "level": "warn", "appender": "p2p"}',
            'Logger definition json: {"name", "level", "appender"}'
        )

        self.config.add_entry(
            'backtrace',
            'yes',
            'Whether to print backtrace on SIGSEGV'
        )

        self.add_plugin('account_by_key')
        self.add_plugin('account_by_key_api')
        self.add_plugin('condenser_api')
        self.add_plugin('network_broadcast_api')
        self.add_plugin('network_node_api')

        self.config.add_entry(
            'shared-file-dir',
            '"blockchain"',
            'The location of the chain shared memory files (absolute path or relative to application data dir)'
        )

        self.config.add_entry(
            'shared-file-size',
            '6G',
            'Size of the shared memory file. Default: 54G. If running a full node, increase this value to 200G'
        )

        self.config.add_entry(
            'enable-stale-production',
            '1',
            'Enable block production, even if the chain is stale'
        )

        self.config.add_entry(
            'required-participation',
            '0',
            'Percent of witnesses (0-99) that must be participating in order to produce blocks'
        )

    def __str__(self):
        return f'{self.network.name}::{self.name}'

    def __del__(self):
        if not self.is_running():
            return

        self.close()
        self.wait_for_close()

    def add_plugin(self, plugin):
        supported_plugins = {
            'account_by_key',
            'account_by_key_api',
            'condenser_api',
            'network_broadcast_api',
            'network_node_api',
            'witness',
        }

        if plugin not in supported_plugins:
            raise Exception(f'Plugin {plugin} is not supported')

        self.config.add_entry(
            'plugin',
            plugin,
            'Plugin(s) to enable, may be specified multiple times',
        )

    def set_directory(self, directory):
        self.directory = Path(directory).absolute()

    def is_running(self):
        if not self.process:
            return False

        return self.process.poll() is None

    def get_name(self):
        return self.name

    def get_p2p_endpoints(self):
        if 'p2p-endpoint' not in self.config:
            self.add_p2p_endpoint(f'0.0.0.0:{self.network.allocate_port()}')

        return self.config['p2p-endpoint']

    def add_p2p_endpoint(self, endpoint):
        self.config.add_entry('p2p-endpoint', endpoint)

    def add_webserver_http_endpoint(self, endpoint):
        self.config.add_entry('webserver-http-endpoint', endpoint)

    def get_webserver_http_endpoints(self):
        return self.config['webserver-http-endpoint']

    def add_webserver_ws_endpoint(self, endpoint):
        self.config.add_entry('webserver-ws-endpoint', endpoint)

    def get_webserver_ws_endpoints(self):
        return self.config['webserver-ws-endpoint']

    def add_seed_node(self, seed_node):
        endpoints = seed_node.get_p2p_endpoints()

        if len(endpoints) == 0:
            raise Exception(f'Cannot connect {self} to {seed_node}; has no endpoints')

        endpoint = endpoints[0]
        port = endpoint.split(':')[1]

        self.config.add_entry(
            'p2p-seed-node',
            f'127.0.0.1:{port}',
        )

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

        if not self.get_webserver_http_endpoints():
            raise Exception('Webserver http endpoint is unknown')

        from urllib.parse import urlparse
        endpoint = f'http://{urlparse(self.get_webserver_http_endpoints()[0], "http").path}'

        if '0.0.0.0' in endpoint:
            endpoint = endpoint.replace('0.0.0.0', '127.0.0.1')

        while not self.is_http_listening():
            time.sleep(1)

        from . import communication
        success, response = communication.request(endpoint, message)

        if not success:
            raise Exception(f'Unknown communication error occurred, response: {response}')

        return response

    def get_id(self):
        response = self.send('network_node_api.get_info')
        return response['result']['node_id']

    def set_allowed_nodes(self, nodes):
        return self.send(
            'network_node_api.set_allowed_peers',
            {
                'allowed_peers': [node.get_id() for node in nodes],
            }
        )

    def run(self):
        if not self.executable_file_path:
            from .paths_to_executables import get_hived_path
            self.executable_file_path = get_hived_path()

        self.directory.mkdir(parents=True)

        config_file_path = self.directory.joinpath('config.ini')
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

        print(f'[{self}] Run with pid {self.process.pid}, with http server {self.get_webserver_http_endpoints()[0]}')

    def close(self):
        self.process.kill()

    def wait_for_close(self):
        self.process.wait()

    def set_executable_file_path(self, executable_file_path):
        self.executable_file_path = executable_file_path

    def set_witness(self, witness_name, key=None):
        if 'witness' not in self.config['plugin']:
            self.add_plugin('witness')

        if key is None:
            witness = Witness(witness_name)
            key = witness.private_key

        self.config.add_entry(
            'witness',
            f'"{witness_name}"',
            'Name of witness controlled by this node'
        )

        self.config.add_entry(
            'private-key',
            key
        )
