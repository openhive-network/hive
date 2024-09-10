from __future__ import annotations

import socket
from json import loads
from typing import TYPE_CHECKING, Any

from helpy import Settings
from helpy.communicators import AioHttpCommunicator, RequestCommunicator

if TYPE_CHECKING:
    from helpy import HttpUrl

    from schemas.jsonrpc import JSONRPCRequest


async def async_raw_http_call(*, http_endpoint: HttpUrl, data: JSONRPCRequest) -> dict[str, Any]:
    """Make raw call with given data to given http_endpoint."""
    communicator = AioHttpCommunicator(settings=Settings(http_endpoint=http_endpoint))
    response = await communicator.async_send(url=http_endpoint, data=data.json(by_alias=True))
    return loads(response)


def raw_http_call(*, http_endpoint: HttpUrl, data: JSONRPCRequest) -> dict[str, Any]:
    """Make raw call with given data to given http_endpoint."""
    communicator = RequestCommunicator(settings=Settings(http_endpoint=http_endpoint))
    response = communicator.send(url=http_endpoint, data=data.json(by_alias=True))
    return loads(response)


def get_port() -> int:
    """Return free port."""
    sock = socket.socket()
    sock.bind(("", 0))
    return int(sock.getsockname()[1])
