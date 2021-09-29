from pathlib import Path
import shutil
import subprocess
import warnings

from test_tools import paths_to_executables

class BlockLog:
    def __init__(self, owner, path, *, include_index=True):
        self.__owner = owner
        self.__path = Path(path)
        self.__include_index = include_index

    def __repr__(self):
        return f'<BlockLog: path={self.__path}>'

    def get_path(self):
        return self.__path

    def copy_to(self, destination):
        shutil.copy(self.__path, destination)

        if self.__include_index:
            block_log_index_file_name = f'{self.__path.name}.index'
            block_log_index_path = self.__path.with_name(block_log_index_file_name)
            if block_log_index_path.exists():
                shutil.copy(block_log_index_path, destination)
            else:
                self.__warn_about_missing_index(block_log_index_path)

    def truncate(self, output_block_log_path: str, block_number: int):
        executable_file_path = paths_to_executables.get_path_of('truncate_block_log')
        subprocess.run(
            [
                str(executable_file_path),
                self.__path,
                output_block_log_path,
                str(block_number),
            ],
            check=True,
        )
        return BlockLog(None, output_block_log_path)

    def __warn_about_missing_index(self, block_log_index_path):
        if self.__owner is not None:
            hint_for_excluding_index = 'node.get_block_log(include_index=False)'
        else:
            hint_for_excluding_index = f'from test_tools import BlockLog\n' \
                                       f'block_log = BlockLog(\'{self.__path}\', include_index=False)'

        warnings.warn(
            f'Block log with following path is missing:\n'
            f'{block_log_index_path}\n'
            f'\n'
            f'If you want to use block log without index, set include_index flag to False like:\n'
            f'{hint_for_excluding_index}'
        )
