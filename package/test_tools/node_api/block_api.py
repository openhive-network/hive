from test_tools.node_api.api_base import ApiBase


class BlockApi(ApiBase):
    def __init__(self, node):
        super().__init__(node, 'block_api')
