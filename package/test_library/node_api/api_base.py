class ApiBase:
    def __init__(self, node, name):
        self.__name = name
        self.__node = node

    def _send(self, method, params=None, jsonrpc='2.0', id=1):
        return self.__node.send(f'{self.__name}.{method}', params, jsonrpc=jsonrpc, id=id)
