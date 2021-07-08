import json
from pathlib import Path


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

    def __repr__(self):
        return f'Snapshot from {self.__creator}'
