import filecmp
import json
import os
import shutil

from pathlib import Path
from distutils.util import strtobool

PACKAGE_PATH = Path(__file__).parent


def dumped_directory_path(extension, type_of_serialization, pattern_name):
    return PACKAGE_PATH / f'dumped_{extension}_files_{type_of_serialization}_wallet' / f'{pattern_name}.{extension}'


def verify_correctness_of_generated_transaction_json(wallet, pattern_name):
    source_path_file = wallet.directory / f'{pattern_name}.json'
    target_path_file = dumped_directory_path('json', wallet.transaction_serialization, pattern_name)

    # Compare actual transaction with stored transaction
    with open(source_path_file) as file:
        actual_json = json.load(file)
    with open(target_path_file) as file:
        pattern_json = json.load(file)
    assert actual_json == pattern_json


def verify_correctness_of_generated_transaction_bin(wallet, pattern_name):
    source_path_file = wallet.directory / f'{pattern_name}.bin'
    target_path_file = dumped_directory_path('bin', wallet.transaction_serialization, pattern_name)

    # Compare actual transaction with stored transaction
    assert filecmp.cmp(source_path_file, target_path_file, shallow=False)


def store_transaction(wallet, pattern_name):
    for file_extension in ['json', 'bin']:
        source_path_file = wallet.directory / f'{pattern_name}.{file_extension}'
        target_path_file = dumped_directory_path(file_extension, wallet.transaction_serialization, pattern_name)

        shutil.move(source_path_file, target_path_file)


def compare_with_pattern(validate_function, wallet, pattern_name):
    generate_patterns = strtobool(os.environ.get('GENERATE_PATTERNS', 'OFF'))
    if not generate_patterns:
        validate_function(wallet, pattern_name)
    else:
        store_transaction(wallet, pattern_name)
