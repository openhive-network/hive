import filecmp
import json
import os
import pathlib
import shutil
from distutils.util import strtobool


def move_dumped_files(wallet, files_name):
    source_path_json_file = wallet.directory / f'{files_name}.json'
    target_path_json_file = pathlib.Path(__file__).parent.absolute() / 'dump_json_files' / f'{files_name}.json'

    source_path_binary_file = wallet.directory / f'{files_name}.bin'
    target_path_binary_file = pathlib.Path(__file__).parent.absolute() / 'dump_binary_files' / f'{files_name}.bin'

    shutil.move(source_path_json_file, target_path_json_file)
    shutil.move(source_path_binary_file, target_path_binary_file)


def import_private_keys_from_json_file(wallet):
    with open(pathlib.Path(__file__).parent / 'block_log' / 'private_keys.json') as file:
        private_keys = json.load(file)
    wallet.api.import_keys(list(private_keys.values()))


def verify_correctness_of_generated_transaction_json(wallet, type_of_serialization, request, pattern_name):
    source_path_file = wallet.directory / f'{pattern_name}.json'
    target_path_file = pathlib.Path(
        request.fspath.dirname) / f'dumped_json_files_{type_of_serialization}_wallet' / f'{pattern_name}.json'

    # Compare actual transaction with stored transaction
    with open(source_path_file) as file:
        actual_json = json.load(file)
    with open(target_path_file) as file:
        pattern_json = json.load(file)
    assert actual_json == pattern_json


def verify_correctness_of_generated_transaction_bin(wallet, type_of_serialization, request, pattern_name):
    source_path_file = wallet.directory / f'{pattern_name}.bin'
    target_path_file = pathlib.Path(
        request.fspath.dirname) / f'dumped_bin_files_{type_of_serialization}_wallet' / f'{pattern_name}.bin'

    # Compare actual transaction with stored transaction
    assert filecmp.cmp(source_path_file, target_path_file, shallow=False)


def store_transaction(wallet, type_of_serialization, request, pattern_name):
    for file_extension in ['json', 'bin']:
        source_path_file = wallet.directory / f'{pattern_name}.{file_extension}'
        target_path_file = pathlib.Path(
            request.fspath.dirname) / f'dumped_{file_extension}_files_{type_of_serialization}_wallet' / f'{pattern_name}.{file_extension}'

        shutil.move(source_path_file, target_path_file)


def compare_with_pattern(validate_function, wallet, type_of_serialization, request, pattern_name):
    generate_patterns = strtobool(os.environ.get('GENERATE_PATTERNS', 'OFF'))
    if not generate_patterns:
        validate_function(wallet, type_of_serialization, request, pattern_name)
    else:
        store_transaction(wallet, type_of_serialization, request, pattern_name)
