from .api_base import ApiBase


class NetworkNodeApi(ApiBase):
    def __init__(self, node):
        super().__init__(node, 'network_node_api')

    def get_info(self):
        return self._send('get_info')

    def set_allowed_peers(self, allowed_peers):
        return self._send(
            'set_allowed_peers',
            {
                'allowed_peers': allowed_peers,
            }
        )
