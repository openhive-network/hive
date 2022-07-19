import filecmp
import json
import os
import shutil

from pathlib import Path
from distutils.util import strtobool

PACKAGE_PATH = Path(__file__).parent


def dumped_directory_path(extension, type_of_serialization, pattern_name):
    return PACKAGE_PATH / f'dumped_files_{type_of_serialization}_wallet' / f'{pattern_name}.{extension}'


def __able_to_generate_pattern(validate_function):
    def impl(wallet, pattern_name):
        generate_patterns = strtobool(os.environ.get('GENERATE_PATTERNS', 'OFF'))
        if not generate_patterns:
            return validate_function(wallet, pattern_name)

        for file_extension in ['json', 'bin']:
            shutil.move(
                wallet.directory / f'{pattern_name}.{file_extension}',
                dumped_directory_path(file_extension, wallet.transaction_serialization, pattern_name)
            )
    return impl


@__able_to_generate_pattern
def verify_generated_transaction_with_json_pattern(wallet, pattern_name):
    source_path_file = wallet.directory / f'{pattern_name}.json'
    target_path_file = dumped_directory_path('json', wallet.transaction_serialization, pattern_name)

    # Compare actual transaction with stored transaction
    with open(source_path_file) as file:
        actual_json = json.load(file)
    with open(target_path_file) as file:
        pattern_json = json.load(file)
    assert actual_json == pattern_json


@__able_to_generate_pattern
def verify_generated_transaction_with_binary_pattern(wallet, pattern_name):
    source_path_file = wallet.directory / f'{pattern_name}.bin'
    target_path_file = dumped_directory_path('bin', wallet.transaction_serialization, pattern_name)

    # Compare actual transaction with stored transaction
    assert filecmp.cmp(source_path_file, target_path_file, shallow=False)
