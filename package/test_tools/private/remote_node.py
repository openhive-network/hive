from test_tools import communication
from test_tools.node_api.node_apis import Apis
from test_tools.private.node_message import NodeMessage


class RemoteNode:
    def __init__(self, endpoint):
        self.api = Apis(self)
        self.__endpoint = endpoint

    def send(self, method, params=None, jsonrpc='2.0', id_=1):
        return communication.request(
            self.__endpoint,
            NodeMessage(method, params, jsonrpc, id_).as_json()
        )
