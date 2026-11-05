"""
Deprecated - the objects of `hiveio_api_client._communication.url` under their former path in hiveio_api,
kept for packages importing it. Use `hiveio_api.client` (`hiveio_api_client` for the names it does not export) instead.
"""

from __future__ import annotations

from hiveio_api_client._communication.url import (
    AnyUrl,
    HttpProtocolT,
    HttpUrl,
    P2PProtocolT,
    P2PUrl,
    ProtocolT,
    Url,
    WsProtocolT,
    WsUrl,
)

__all__ = [
    "AnyUrl",
    "HttpProtocolT",
    "HttpUrl",
    "P2PProtocolT",
    "P2PUrl",
    "ProtocolT",
    "Url",
    "WsProtocolT",
    "WsUrl",
]
