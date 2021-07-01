from test_tools.node_api.api_base import ApiBase


class NetworkBroadcastApi(ApiBase):
    def __init__(self, node):
        super().__init__(node, 'network_broadcast_api')
