import ast
from pathlib import Path
import subprocess


class KeyGenerator:
    def __init__(self, executable_path):
        self.executable_path = Path(executable_path).absolute()

    def generate_keys(self, account_name, secret='secret'):
        if self.executable_path is None:
            raise Exception('Missing executable, use KeyGenerator.set_executable_path')

        if account_name == 'initminer':
            return {
                'private_key': '5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n',
                'public_key': 'TST6LLegbAgLAy28EHrffBVuANFWcFgmqRMW13wBmTExqFE9SCkg4',
                'account_name': account_name,
            }

        output = subprocess.check_output([str(self.executable_path), secret, account_name]).decode('utf-8')
        return ast.literal_eval(output)[0]
