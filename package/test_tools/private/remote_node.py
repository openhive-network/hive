from typing import Optional

from test_tools import communication
from test_tools.node_api.node_apis import Apis
from test_tools.private.node_message import NodeMessage
from test_tools.private.scope import context
from test_tools.private.url import Url


class RemoteNode:
    def __init__(self, http_endpoint: str, *, ws_endpoint: Optional[str] = None):
        self.api = Apis(self)
        self.name = context.names.register_numbered_name('RemoteNode')
        self.__http_endpoint: Url = Url(http_endpoint, protocol='http')
        self.__ws_endpoint: Optional[Url] = Url(ws_endpoint, protocol='ws') if ws_endpoint is not None else None

    def __str__(self) -> str:
        return self.name

    def send(self, method, params=None, jsonrpc='2.0', id_=1):
        return communication.request(
            self.__http_endpoint.as_string(),
            NodeMessage(method, params, jsonrpc, id_).as_json()
        )

    def get_ws_endpoint(self):
        if self.__ws_endpoint is None:
            return None

        return self.__ws_endpoint.as_string(with_protocol=False)

    @staticmethod
    def is_running():
        return True
