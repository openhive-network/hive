from test_tools.node_api.api_base import ApiBase


class AccountHistoryApi(ApiBase):
    def __init__(self, node):
        super().__init__(node, 'account_history_api')
