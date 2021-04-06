from .database_api import DatabaseApi
from .network_node_api import NetworkNodeApi


class Apis:
    def __init__(self, node):
        self.__node = node

        self.database = DatabaseApi(self.__node)
        self.network_node = NetworkNodeApi(self.__node)
