from test_tools.node_api.api_base import ApiBase


class JsonrpcApi(ApiBase):
    def __init__(self, node):
        super().__init__(node, 'jsonrpc')
