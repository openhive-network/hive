from .api_base import ApiBase


class NetworkNodeApi(ApiBase):
    def __init__(self, node):
        super().__init__(node, 'network_node_api')
