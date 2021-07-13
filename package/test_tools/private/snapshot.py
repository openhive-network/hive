import json
from pathlib import Path
import shutil


class Snapshot:
    def __init__(self, snapshot_path, block_log_path, block_log_index_path, node=None):
        self.__snapshot_path = snapshot_path
        self.__block_log_path = block_log_path
        self.__block_log_index_path = block_log_index_path
        self.__creator = node

        self.state = None
        if node is not None:
            with open(node.directory / 'state_snapshot_dump.json') as state_file:
                self.state = json.load(state_file)

    def copy_to(self, node_directory: Path):
        blocklog_directory = node_directory / 'blockchain'
        blocklog_directory.mkdir(exist_ok=True)

        if self.__block_log_path.parent != blocklog_directory:
            shutil.copy(self.__block_log_path, blocklog_directory)

        if self.__block_log_index_path.parent != blocklog_directory:
            shutil.copy(self.__block_log_index_path, blocklog_directory)

        destination_snapshot_path = node_directory / 'snapshot'
        if self.__snapshot_path != destination_snapshot_path:
            shutil.copytree(self.__snapshot_path, destination_snapshot_path)

    def __repr__(self):
        optional_creator_info = '' if self.__creator is None else f' from {self.__creator}'
        return f'<Snapshot{optional_creator_info}: path={self.__snapshot_path}>'
