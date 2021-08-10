from test_tools import communication
from test_tools.node_api.node_apis import Apis


class RemoteNode:
    def __init__(self, endpoint):
        self.api = Apis(self)
        self.__endpoint = endpoint

    def _send(self, method, params=None, jsonrpc='2.0', id_=1):
        message = {
            'jsonrpc': jsonrpc,
            'id': id_,
            'method': method,
        }

        if params is not None:
            message['params'] = params

        return communication.request(self.__endpoint, message)
