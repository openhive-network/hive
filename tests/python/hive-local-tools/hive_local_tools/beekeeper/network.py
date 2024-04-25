from __future__ import annotations

import socket
from typing import TYPE_CHECKING, Any

import aiohttp

if TYPE_CHECKING:
    from clive.core.url import Url
    from schemas.jsonrpc import JSONRPCRequest


async def raw_http_call(*, http_endpoint: Url, data: JSONRPCRequest) -> dict[str, Any]:
    """Make raw async call with given data to given http_endpoint."""
    data_serialized = data.json(by_alias=True)
    async with aiohttp.ClientSession() as session, session.post(
        http_endpoint.as_string(), data=data_serialized
    ) as resp:
        return dict(await resp.json())


def get_port() -> int:
    """Return free port."""
    sock = socket.socket()
    sock.bind(("", 0))
    return int(sock.getsockname()[1])
