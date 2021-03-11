from pathlib import Path
import subprocess
import signal

from node import Node


class Wallet:
    def __init__(self, directory=Path()):
        self.server_websocket_rpc_endpoint = None

        self.directory = directory
        self.executable_file_path = None
        self.stdout_file = None
        self.stderr_file = None
        self.process = None

    def is_running(self):
        if not self.process:
            return False

        return self.process.poll() is None

    def run(self):
        if not self.executable_file_path:
            raise Exception('Missing executable')

        if not self.server_websocket_rpc_endpoint:
            raise Exception('Server websocket RPC endpoint not set, use Wallet.connect_to method')

        self.directory.mkdir()

        print('[Wallet] self.executable_file_path =', self.executable_file_path)
        print('[Wallet] self.directory =', self.directory.absolute())

        self.stdout_file = open(self.directory / 'stdout.txt', 'w')
        self.stderr_file = open(self.directory / 'stderr.txt', 'w')

        # ./cli_wallet --chain-id=04e8b5fc4bb4ab3c0ee3584199a2e584bfb2f141222b3a0d1c74e8a75ec8ff39 -s ws://0.0.0.0:3903
        #              -d -H 0.0.0.0:3904 --rpc-http-allowip 192.168.10.10 --rpc-http-allowip=127.0.0.1

        self.process = subprocess.Popen(
            [
                self.executable_file_path,
                '--chain-id=04e8b5fc4bb4ab3c0ee3584199a2e584bfb2f141222b3a0d1c74e8a75ec8ff39',
                '-s', f'ws://{self.server_websocket_rpc_endpoint}',
                '-d',
                '-H', '0.0.0.0:3904',
                '--rpc-http-allowip', '192.168.10.10',
                '--rpc-http-allowip=127.0.0.1'
            ],
            cwd=self.directory,
            stdout=self.stdout_file,
            stderr=self.stderr_file
        )

        print(f'Wallet run with pid {self.process.pid}')

    def connect_to(self, node: Node):
        self.server_websocket_rpc_endpoint = node.get_webserver_ws_endpoints()[0]

    def close(self):
        self.process.send_signal(signal.SIGINT)

    def wait_for_close(self):
        return_code = self.process.wait()
        print(f'[Wallet] Closed with {return_code} return code')

    def set_executable_file_path(self, executable_file_path):
        self.executable_file_path = executable_file_path
