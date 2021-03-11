from pathlib import Path
import subprocess


class Wallet:
    def __init__(self, directory=Path()):
        self.directory = directory
        self.executable_file_path = None
        self.stdout_file = None
        self.stderr_file = None
        self.process = None

    def run(self):
        if not self.executable_file_path:
            raise Exception('Missing executable')

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
                '-s', 'ws://0.0.0.0:3903',
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

    def close(self):
        self.process.kill()

    def wait_for_close(self):
        return_code = self.process.wait()
        print(f'[Wallet] Closed with {return_code} return code')

    def set_executable_file_path(self, executable_file_path):
        self.executable_file_path = executable_file_path
