from test_tools.node_api.api_base import ApiBase, NodeApiCallProxy


class CondenserApiCallProxy(NodeApiCallProxy):
    @staticmethod
    def _prepare_params(*args, **kwargs):
        return list(args)


class CondenserApi(ApiBase):
    _NodeApiCallProxyType = CondenserApiCallProxy

    def __init__(self, node):
        super().__init__(node, 'condenser_api')
