import ast
import subprocess

from test_tools import paths_to_executables


class KeyGenerator:
    def __init__(self, executable_path=None):
        self.executable_path = executable_path

    def generate_keys(self, account_name, *, number_of_accounts=1, secret='secret'):
        assert number_of_accounts >= 1

        if self.executable_path is None:
            self.executable_path = paths_to_executables.get_path_of('get_dev_key')

        if account_name == 'initminer':
            assert number_of_accounts == 1
            return [{
                'private_key': '5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n',
                'public_key': 'TST6LLegbAgLAy28EHrffBVuANFWcFgmqRMW13wBmTExqFE9SCkg4',
                'account_name': account_name,
            }]

        if number_of_accounts != 1:
            account_name += f'-0:{number_of_accounts}'

        output = subprocess.check_output([str(self.executable_path), secret, account_name]).decode('utf-8')
        return ast.literal_eval(output)
