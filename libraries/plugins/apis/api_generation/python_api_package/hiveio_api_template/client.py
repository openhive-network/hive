"""
Infrastructure of API clients - base classes of APIs and API collections, their owners (`SyncSendable`,
`AsyncSendable`), callbacks of requests, requests and responses, JSON-RPC, URLs and builtins of parameters of API calls
(`to_api_builtins`).

The public interface for libraries building handles on top of the generated clients (e.g. beekeepy) - names are
imported from their modules when used for the first time.
"""

from __future__ import annotations

from typing import TYPE_CHECKING

from hiveio_api._utilities.smart_lazy_import import (
    aggregate_same_import,
    lazy_module_factory,
)

if TYPE_CHECKING:
    from hiveio_api._apis.abc.api import (
        AbstractAsyncApi,
        AbstractSyncApi,
        ApiArgumentSerialization,
        ApiArgumentsToSerialize,
        HandleT,
        RegisteredApisT,
    )
    from hiveio_api._apis.abc.api_collection import (
        AbstractAsyncApiCollection,
        AbstractSyncApiCollection,
    )
    from hiveio_api._apis.abc.sendable import (
        AsyncSendable,
        SyncSendable,
    )
    from hiveio_api._base import to_api_builtins
    from hiveio_api._communication.abc.communicator_models import (
        AsyncCallback,
        AsyncCallbacks,
        AsyncErrorCallback,
        AsyncRequestCallback,
        AsyncResponseCallback,
        Callbacks,
        ErrorCallback,
        Methods,
        Request,
        RequestCallback,
        RequestContext,
        RequestTimings,
        Response,
        ResponseCallback,
        SyncCallback,
    )
    from hiveio_api._communication.jsonrpc import (
        ExpectResultT,
        JSONRPCError,
        JSONRPCRequest,
        JSONRPCResult,
        build_response,
    )
    from hiveio_api._communication.url import (
        AnyUrl,
        HttpUrl,
        P2PUrl,
        Url,
        WsUrl,
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

__getattr__ = lazy_module_factory(
    globals(),
    *aggregate_same_import(
        "AbstractAsyncApi",
        "AbstractSyncApi",
        "ApiArgumentSerialization",
        "ApiArgumentsToSerialize",
        "HandleT",
        "RegisteredApisT",
        module="hiveio_api._apis.abc.api",
    ),
    *aggregate_same_import(
        "AbstractAsyncApiCollection",
        "AbstractSyncApiCollection",
        module="hiveio_api._apis.abc.api_collection",
    ),
    *aggregate_same_import(
        "AsyncSendable",
        "SyncSendable",
        module="hiveio_api._apis.abc.sendable",
    ),
    ("hiveio_api._base", "to_api_builtins"),
    *aggregate_same_import(
        "AsyncCallback",
        "AsyncCallbacks",
        "AsyncErrorCallback",
        "AsyncRequestCallback",
        "AsyncResponseCallback",
        "Callbacks",
        "ErrorCallback",
        "Methods",
        "Request",
        "RequestCallback",
        "RequestContext",
        "RequestTimings",
        "Response",
        "ResponseCallback",
        "SyncCallback",
        module="hiveio_api._communication.abc.communicator_models",
    ),
    *aggregate_same_import(
        "ExpectResultT",
        "JSONRPCError",
        "JSONRPCRequest",
        "JSONRPCResult",
        "build_response",
        module="hiveio_api._communication.jsonrpc",
    ),
    *aggregate_same_import(
        "AnyUrl",
        "HttpUrl",
        "P2PUrl",
        "Url",
        "WsUrl",
        module="hiveio_api._communication.url",
    ),
)
