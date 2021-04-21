import os
import shutil
from pathlib import Path

from .paths_to_executables_implementation import NotSupported

HIVED_PATH_ENVIRONMENT_VARIABLE_NAME = 'HIVED_EXECUTABLE'
CLI_WALLET_PATH_ENVIRONMENT_VARIABLE_NAME = 'CLI_WALLET_EXECUTABLE'
KEY_GENERATOR_PATH_ENVIRONMENT_VARIABLE_NAME = 'KEY_GENERATOR_EXECUTABLE'

def __get_environment_variable(variable_name):
    if variable_name not in os.environ:
        __show_message_about_missing_variables()
        raise Exception(f'Environment variable {variable_name} not set. Read message printed above traceback for details.')

    return Path(os.path.expandvars(os.environ[variable_name]))

def __show_message_about_missing_variables():
    print('Variables listed below are missing. Set them in /etc/environment with correct paths.\n')

    EXAMPLE_HIVE_BUILD_PATH_ENVIRONMENT_VARIABLE_NAME = 'HIVE_BUILD_PATH'
    print(f'{EXAMPLE_HIVE_BUILD_PATH_ENVIRONMENT_VARIABLE_NAME}=\'/home/dev/hive/build\'')

    if HIVED_PATH_ENVIRONMENT_VARIABLE_NAME not in os.environ:
        print(f'{HIVED_PATH_ENVIRONMENT_VARIABLE_NAME}=\'${{{EXAMPLE_HIVE_BUILD_PATH_ENVIRONMENT_VARIABLE_NAME}}}/programs/hived/hived\'')

    if CLI_WALLET_PATH_ENVIRONMENT_VARIABLE_NAME not in os.environ:
        print(f'{CLI_WALLET_PATH_ENVIRONMENT_VARIABLE_NAME}=\'${{{EXAMPLE_HIVE_BUILD_PATH_ENVIRONMENT_VARIABLE_NAME}}}/programs/cli_wallet/cli_wallet\'')

    if KEY_GENERATOR_PATH_ENVIRONMENT_VARIABLE_NAME not in os.environ:
        print(f'{KEY_GENERATOR_PATH_ENVIRONMENT_VARIABLE_NAME}=\'${{{EXAMPLE_HIVE_BUILD_PATH_ENVIRONMENT_VARIABLE_NAME}}}/programs/util/get_dev_key\'')

    print()

def get_hived_path():
    path = shutil.which('hived')
    if path is not None:
        return Path(path).absolute()

    return __get_environment_variable(HIVED_PATH_ENVIRONMENT_VARIABLE_NAME)

def get_cli_wallet_path():
    path = shutil.which('cli_wallet')
    if path is not None:
        return Path(path).absolute()

    return __get_environment_variable(CLI_WALLET_PATH_ENVIRONMENT_VARIABLE_NAME)

def get_key_generator_path():
    path = shutil.which('get_dev_key')
    if path is not None:
        return Path(path).absolute()

    return __get_environment_variable(KEY_GENERATOR_PATH_ENVIRONMENT_VARIABLE_NAME)
