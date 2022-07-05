import json
import pathlib
import shutil


def move_dumped_files(wallet, files_name):
    source_path_json_file = wallet.directory / f'{files_name}.json'
    target_path_json_file = pathlib.Path(__file__).parent.absolute() / 'dump_json_files' / f'{files_name}.json'

    source_path_binary_file = wallet.directory / f'{files_name}.bin'
    target_path_binary_file = pathlib.Path(__file__).parent.absolute() / 'dump_binary_files' / f'{files_name}.bin'

    shutil.move(source_path_json_file, target_path_json_file)
    shutil.move(source_path_binary_file, target_path_binary_file)


def read_keys_from_json_file():
    with open(pathlib.Path(__file__).parent.joinpath('block_log/private_keys.json')) as file:
        keys = json.load(file)

    return keys


def import_keys(wallet):
    private_keys = read_keys_from_json_file()
    wallet.api.import_keys(list(private_keys.values()))
