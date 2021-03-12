from pathlib import Path
from shutil import rmtree

from node import Node


class Network:
    def __init__(self, name):
        self.name = name
        self.directory = Path('.').absolute()
        self.nodes = []

        self.hived_executable_file_path = None

    def set_directory(self, directory):
        self.directory = Path(directory).absolute()

    def set_hived_executable_file_path(self, path):
        self.hived_executable_file_path = Path(path).absolute()

    def get_directory(self):
        return self.directory / self.name

    def add_node(self, node_name):
        node = Node(name=node_name, directory=self.get_directory() / node_name)
        self.nodes.append(node)
        return node

    def run(self):
        print('Script is run in', Path('.').absolute())

        if not self.hived_executable_file_path:
            raise Exception('Missing hived executable, use Network.set_hived_executable_file_path')

        directory = self.get_directory()
        if directory.exists():
            print('Clear whole directory', directory)
            rmtree(directory)

        directory.mkdir(parents=True)

        for node in self.nodes:
            node.set_executable_file_path(self.hived_executable_file_path)
            node.run()
