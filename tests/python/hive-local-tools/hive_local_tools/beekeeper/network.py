from __future__ import annotations

import socket
from typing import TYPE_CHECKING, Any

import requests

if TYPE_CHECKING:
    from helpy import HttpUrl

    from schemas.jsonrpc import JSONRPCRequest


def raw_http_call(*, http_endpoint: HttpUrl, data: JSONRPCRequest) -> dict[str, Any]:
    """Make raw call with given data to given http_endpoint."""
    data_serialized = data.json(by_alias=True)
    response = requests.post(http_endpoint.as_string(), data=data_serialized)
    return dict(response.json())


def get_port() -> int:
    """Return free port."""
    sock = socket.socket()
    sock.bind(("", 0))
    return int(sock.getsockname()[1])
