import os
from pathlib import Path

HIVED_PATH_ENVIRONMENT_VARIABLE_NAME = 'HIVED_EXECUTABLE'
CLI_WALLET_PATH_ENVIRONMENT_VARIABLE_NAME = 'CLI_WALLET_EXECUTABLE'
KEY_GENERATOR_PATH_ENVIRONMENT_VARIABLE_NAME = 'KEY_GENERATOR_EXECUTABLE'

def __get_environment_variable(variable_name):
    if variable_name not in os.environ:
        raise Exception(f'Environment variable {variable_name} not set')

    return Path(os.environ[variable_name])

def get_hived_path():
    return __get_environment_variable(HIVED_PATH_ENVIRONMENT_VARIABLE_NAME)

def get_cli_wallet_path():
    return __get_environment_variable(CLI_WALLET_PATH_ENVIRONMENT_VARIABLE_NAME)

def get_key_generator_path():
    return __get_environment_variable(KEY_GENERATOR_PATH_ENVIRONMENT_VARIABLE_NAME)
