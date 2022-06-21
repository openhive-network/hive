import pathlib
import shutil


def move_dumped_files(wallet, files_name):
    source_path_json_file = wallet.directory / f'{files_name}.json'
    target_path_json_file = pathlib.Path(__file__).parent.absolute() / 'dump_json_files' / f'{files_name}.json'

    source_path_binary_file = wallet.directory / f'{files_name}.bin'
    target_path_binary_file = pathlib.Path(__file__).parent.absolute() / 'dump_binary_files' / f'{files_name}.bin'

    shutil.move(source_path_json_file, target_path_json_file)
    shutil.move(source_path_binary_file, target_path_binary_file)
