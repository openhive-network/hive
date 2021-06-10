import ast
import subprocess

from test_tools import paths_to_executables


class KeyGenerator:
    def __init__(self, executable_path=None):
        self.executable_path = executable_path

    def generate_keys(self, account_name, *, secret='secret'):
        if self.executable_path is None:
            self.executable_path = paths_to_executables.get_path_of('get_dev_key')

        if account_name == 'initminer':
            return {
                'private_key': '5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n',
                'public_key': 'TST6LLegbAgLAy28EHrffBVuANFWcFgmqRMW13wBmTExqFE9SCkg4',
                'account_name': account_name,
            }

        output = subprocess.check_output([str(self.executable_path), secret, account_name]).decode('utf-8')
        return ast.literal_eval(output)[0]
