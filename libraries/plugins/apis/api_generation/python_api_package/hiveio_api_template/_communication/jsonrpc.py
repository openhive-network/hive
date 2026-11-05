"""
Deprecated - the objects of `hiveio_api_client._communication.jsonrpc` under their former path in hiveio_api,
kept for packages importing it. Use `hiveio_api.client` (`hiveio_api_client` for the names it does not export) instead.
"""

from __future__ import annotations

from hiveio_api_client._communication.jsonrpc import (
    ExpectResultT,
    JSONRPCError,
    JsonRpcIdT,
    JSONRPCRequest,
    JSONRPCResult,
    build_response,
)
from hiveio_api_client._communication.jsonrpc import jsonrpc_id as _jsonrpc_id

__all__ = [
    "ExpectResultT",
    "JSONRPCError",
    "JSONRPCRequest",
    "JSONRPCResult",
    "JsonRpcIdT",
    "_jsonrpc_id",
    "build_response",
]
