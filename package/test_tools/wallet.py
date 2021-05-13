from pathlib import Path
import subprocess
import signal
import time

from .account import Account
from . import logger


class Wallet:
    class __Api:
        def __init__(self, wallet):
            self.__wallet = wallet

        def __send(self, method, *params, jsonrpc='2.0', id=0):
            return self.__wallet.send(method, *params, jsonrpc=jsonrpc, id=id)

    def __init__(self, name, creator, directory=Path()):
        self.api = Wallet.__Api(self)
        self.http_server_port = None
        self.connected_node = None
        self.password = None

        self.creator = creator
        self.name = name
        self.directory = directory / self.name
        self.executable_file_path = None
        self.stdout_file = None
        self.stderr_file = None
        self.process = None
        self.finalizer = None
        self.logger = logger.getLogger(f'{__name__}.{self.creator}.{self.name}')

    @staticmethod
    def __close_process(process, logger_from_wallet):
        if not process:
            return

        process.send_signal(signal.SIGINT)

        try:
            return_code = process.wait(timeout=3)
            logger_from_wallet.debug(f'Closed with {return_code} return code')
        except subprocess.TimeoutExpired:
            process.send_signal(signal.SIGKILL)
            logger_from_wallet.warning(f"Send SIGKILL because process didn't close before timeout")

    def get_stdout_file_path(self):
        return self.directory / 'stdout.txt'

    def get_stderr_file_path(self):
        return self.directory / 'stderr.txt'

    def is_running(self):
        if not self.process:
            return False

        return self.process.poll() is None

    def __is_ready(self):
        with open(self.get_stderr_file_path()) as file:
            for line in file:
                if 'Entering Daemon Mode, ^C to exit' in line:
                    return True

        return False

    def run(self):
        if not self.executable_file_path:
            from . import paths_to_executables
            self.executable_file_path = paths_to_executables.get_path_of('cli_wallet')

        if not self.connected_node:
            raise Exception('Server websocket RPC endpoint not set, use Wallet.connect_to method')

        if not self.http_server_port:
            from .port import Port
            self.http_server_port = Port.allocate()

        import shutil
        shutil.rmtree(self.directory, ignore_errors=True)
        self.directory.mkdir(parents=True)

        self.stdout_file = open(self.get_stdout_file_path(), 'w')
        self.stderr_file = open(self.get_stderr_file_path(), 'w')

        if not self.connected_node.is_ws_listening():
            self.logger.info(f'Waiting for node {self.connected_node} to listen...')

        while not self.connected_node.is_ws_listening():
            time.sleep(1)

        self.process = subprocess.Popen(
            [
                str(self.executable_file_path),
                '--chain-id=04e8b5fc4bb4ab3c0ee3584199a2e584bfb2f141222b3a0d1c74e8a75ec8ff39',
                '-s', f'ws://{self.connected_node.config.webserver_ws_endpoint}',
                '-d',
                '-H', f'0.0.0.0:{self.http_server_port}',
                '--rpc-http-allowip', '192.168.10.10',
                '--rpc-http-allowip=127.0.0.1'
            ],
            cwd=self.directory,
            stdout=self.stdout_file,
            stderr=self.stderr_file
        )

        import weakref
        self.finalizer = weakref.finalize(self, Wallet.__close_process, self.process, self.logger)

        while not self.__is_ready():
            time.sleep(0.1)

        from .communication import CommunicationError
        success = False
        for _ in range(30):
            try:
                self.api.info()

                success = True
                break
            except CommunicationError:
                time.sleep(1)

        if not success:
            raise Exception(f'Problem with starting wallet occurred. See {self.get_stderr_file_path()} for more details.')

        self.api.set_password()
        self.api.unlock()
        self.api.import_key(Account('initminer').private_key)

        self.logger.info(f'Started, listening on port {self.http_server_port}')

    def connect_to(self, node):
        self.connected_node = node

    def close(self):
        self.finalizer()

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

    def create_account(self, name, creator='initminer'):
        account = Account(name)

        self.api.create_account_with_keys(creator, account.name)
        self.api.import_key(account.private_key)

        return account
