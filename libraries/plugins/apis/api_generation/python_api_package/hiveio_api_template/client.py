"""
Infrastructure of API clients - base classes of APIs and API collections, their owners (`SyncSendable`,
`AsyncSendable`), callbacks of requests, requests and responses, JSON-RPC, URLs and builtins of parameters of API calls
(`to_api_builtins`).

The public interface for libraries building handles on top of the generated clients (e.g. beekeepy). The names are the
objects of the runtime of generated clients, `hiveio_api_client` (not copies of them) - a handle implementing
`AsyncSendable` of this module is an owner of clients generated against `hiveio_api_client` too, and vice versa. They are
imported when used for the first time.
"""

from __future__ import annotations

import importlib
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from hiveio_api_client import (
        AbstractAsyncApi,
        AbstractAsyncApiCollection,
        AbstractSyncApi,
        AbstractSyncApiCollection,
        AnyUrl,
        ApiArgumentSerialization,
        ApiArgumentsToSerialize,
        AsyncCallback,
        AsyncCallbacks,
        AsyncErrorCallback,
        AsyncRequestCallback,
        AsyncResponseCallback,
        AsyncSendable,
        Callbacks,
        ErrorCallback,
        ExpectResultT,
        HandleT,
        HttpUrl,
        JSONRPCError,
        JSONRPCRequest,
        JSONRPCResult,
        Methods,
        P2PUrl,
        RegisteredApisT,
        Request,
        RequestCallback,
        RequestContext,
        RequestTimings,
        Response,
        ResponseCallback,
        SyncCallback,
        SyncSendable,
        Url,
        WsUrl,
        build_response,
        to_api_builtins,
    )

__all__ = [
    "AbstractAsyncApi",
    "AbstractAsyncApiCollection",
    "AbstractSyncApi",
    "AbstractSyncApiCollection",
    "AnyUrl",
    "ApiArgumentSerialization",
    "ApiArgumentsToSerialize",
    "AsyncCallback",
    "AsyncCallbacks",
    "AsyncErrorCallback",
    "AsyncRequestCallback",
    "AsyncResponseCallback",
    "AsyncSendable",
    "Callbacks",
    "ErrorCallback",
    "ExpectResultT",
    "HandleT",
    "HttpUrl",
    "JSONRPCError",
    "JSONRPCRequest",
    "JSONRPCResult",
    "Methods",
    "P2PUrl",
    "RegisteredApisT",
    "Request",
    "RequestCallback",
    "RequestContext",
    "RequestTimings",
    "Response",
    "ResponseCallback",
    "SyncCallback",
    "SyncSendable",
    "Url",
    "WsUrl",
    "build_response",
    "to_api_builtins",
]

def __getattr__(name: str) -> object:
    if name in __all__:
        return getattr(importlib.import_module("hiveio_api_client"), name)
    raise AttributeError(f"module {__name__!r} has no attribute {name!r}")
