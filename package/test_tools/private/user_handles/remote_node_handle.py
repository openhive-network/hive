from typing import Optional

from test_tools.private.remote_node import RemoteNode


class RemoteNodeHandle:
    def __init__(self, http_endpoint: str, *, ws_endpoint: Optional[str] = None):
        self.__implementation = RemoteNode(http_endpoint, ws_endpoint=ws_endpoint)
        self.api = self.__implementation.api
