"""
Deprecated - the objects of `hiveio_api_client._apis.abc.endpoint` under their former path in hiveio_api,
kept for packages importing it. Use `hiveio_api_client._apis.abc.endpoint` instead.
"""

from __future__ import annotations

from hiveio_api_client._apis.abc.endpoint import (
    ApiT,
    BoundEndpoint,
    CallbacksT,
    Endpoint,
    P,
    R,
)

__all__ = [
    "ApiT",
    "BoundEndpoint",
    "CallbacksT",
    "Endpoint",
    "P",
    "R",
]
