from test_tools import communication
from test_tools.node_api.node_apis import Apis


class MainNet:
    def __init__(self):
        self.api = Apis(self)

    def _send(self, method, params=None, jsonrpc='2.0', id=1):
        message = {
            'jsonrpc': jsonrpc,
            'id': id,
            'method': method,
        }

        if params is not None:
            message['params'] = params

        return communication.request('https://api.hive.blog:443', message)
