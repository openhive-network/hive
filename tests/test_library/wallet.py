from pathlib import Path
import subprocess
import signal
import time

from .node import Node
from .witness import Witness


class Wallet:
    def __init__(self, directory=Path()):
        self.http_server_port = None
        self.connected_node = None
        self.password = None

        self.directory = directory
        self.executable_file_path = None
        self.stdout_file = None
        self.stderr_file = None
        self.process = None

    def __del__(self):
        if not self.is_running():
            return

        self.close()
        self.wait_for_close()

    def is_running(self):
        if not self.process:
            return False

        return self.process.poll() is None

    def run(self):
        if not self.executable_file_path:
            from .paths_to_executables import get_cli_wallet_path
            self.executable_file_path = get_cli_wallet_path()

        if not self.connected_node:
            raise Exception('Server websocket RPC endpoint not set, use Wallet.connect_to method')

        if not self.http_server_port:
            raise Exception('Http server port is not set, use Wallet.set_http_server_port method')

        self.directory.mkdir(parents=True, exist_ok=True)

        self.stdout_file = open(self.directory / 'stdout.txt', 'w')
        self.stderr_file = open(self.directory / 'stderr.txt', 'w')

        if not self.connected_node.is_ws_listening():
            print(f'[Wallet] Waiting for node {self.connected_node} to listen...')

        while not self.connected_node.is_ws_listening():
            time.sleep(1)

        # ./cli_wallet --chain-id=04e8b5fc4bb4ab3c0ee3584199a2e584bfb2f141222b3a0d1c74e8a75ec8ff39 -s ws://0.0.0.0:3903
        #              -d -H 0.0.0.0:3904 --rpc-http-allowip 192.168.10.10 --rpc-http-allowip=127.0.0.1

        self.process = subprocess.Popen(
            [
                str(self.executable_file_path),
                '--chain-id=04e8b5fc4bb4ab3c0ee3584199a2e584bfb2f141222b3a0d1c74e8a75ec8ff39',
                '-s', f'ws://{self.connected_node.get_webserver_ws_endpoints()[0]}',
                '-d',
                '-H', f'0.0.0.0:{self.http_server_port}',
                '--rpc-http-allowip', '192.168.10.10',
                '--rpc-http-allowip=127.0.0.1'
            ],
            cwd=self.directory,
            stdout=self.stdout_file,
            stderr=self.stderr_file
        )

        print(f'[Wallet] Started with pid {self.process.pid}, listening on port {self.http_server_port}')

    def connect_to(self, node: Node):
        self.connected_node = node

    def close(self):
        self.process.send_signal(signal.SIGINT)

    def wait_for_close(self):
        return_code = self.process.wait()
        print(f'[Wallet] Closed with {return_code} return code')

    def set_executable_file_path(self, executable_file_path):
        self.executable_file_path = executable_file_path

    def set_http_server_port(self, port):
        self.http_server_port = port

    def send(self, method, *params, jsonrpc='2.0', id=0):
        endpoint = f'http://127.0.0.1:{self.http_server_port}'
        message = {
            'jsonrpc': jsonrpc,
            'id': id,
            'method': method,
            'params': list(params)
        }

        from . import communication
        return communication.request(endpoint, message)

    # --- Wallet api calls ----------------------------------------------------
    def info(self):
        return self.send('info')

    def set_password(self, password='default-password'):
        self.password = password
        return self.send('set_password', self.password)

    def unlock(self, password=None):
        return self.send('unlock', self.password if password is None else password)

    def import_key(self, key):
        return self.send('import_key', key)

    def create_account_with_keys(self, creator, new_account_name, json_meta='', owner=None, active=None, posting=None, memo=None, broadcast=True):
        account = Witness(new_account_name)
        return self.send(
            'create_account_with_keys',
            creator,
            new_account_name,
            json_meta,
            owner if owner is not None else account.public_key,
            active if active is not None else account.public_key,
            posting if posting is not None else account.public_key,
            memo if memo is not None else account.public_key,
            broadcast
        )

    def transfer_to_vesting(self, sender, receiver, amount, broadcast=True):
        return self.send('transfer_to_vesting', sender, receiver, amount, broadcast)

    def list_accounts(self, lowerbound='', limit=100):
        return self.send('list_accounts', lowerbound, limit)

    def update_witness(self, witness_name, url, block_signing_key, props, broadcast=True):
        return self.send('update_witness', witness_name, url, block_signing_key, props, broadcast)
