from test_tools.node_api.api_base import ApiBase


class TransactionStatusApi(ApiBase):
    def __init__(self, node):
        super().__init__(node, 'transaction_status_api')
