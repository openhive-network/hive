from test_tools.node_api.api_base import ApiBase


class FollowApi(ApiBase):
    def __init__(self, node):
        super().__init__(node, 'follow_api')
