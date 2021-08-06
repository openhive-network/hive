import pytest

from test_tools.paths_to_executables import _PathsToExecutables


@pytest.fixture
def paths():
    """Returns PathsToExecutables object without any value from current environment.

    Doesn't matter if user has e.g. set environment variable searched by this object or
    script is run with some command line argument. All such information are ignored.
    """

    paths = _PathsToExecutables()
    paths.parse_command_line_arguments([])
    paths.set_environment_variables({})
    paths.set_installed_executables({})
    return paths


def __executables():
    class Executable:
        def __init__(self, name, command_line_argument, environment_variable, default_relative_path):
            self.name = name
            self.path = f'{self.name}_path'
            self.argument = command_line_argument
            self.environment_variable = environment_variable
            self.default_relative_path = default_relative_path

    return [
        Executable('hived', '--hived-path', 'HIVED_PATH', 'programs/hived/hived'),
        Executable('cli_wallet', '--cli-wallet-path', 'CLI_WALLET_PATH', 'programs/cli_wallet/cli_wallet'),
        Executable('get_dev_key', '--get-dev-key-path', 'GET_DEV_KEY_PATH', 'programs/util/get_dev_key'),
    ]


@pytest.fixture
def executables():
    return __executables()


@pytest.fixture
def executable():
    return __executables()[0]
