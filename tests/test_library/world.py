from . import Node
from .children_names import ChildrenNames


class World:
    def __init__(self):
        self.children_names = ChildrenNames()
        self.nodes = []

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        for node in self.nodes:
            if node.is_running():
                node.close()

    def create_node(self, name=None):
        if name is not None:
            self.children_names.register_name(name)
        else:
            name = self.children_names.create_name('Node')

        node = Node(name)
        self.nodes.append(node)
        return node
