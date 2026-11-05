"""
Deprecated - the objects of `hiveio_api_client._communication.abc.communicator_models` under their former path in hiveio_api,
kept for packages importing it. Use `hiveio_api.client` (`hiveio_api_client` for the names it does not export) instead.
"""

from __future__ import annotations

from hiveio_api_client._communication.abc.communicator_models import (
    AsyncCallback,
    AsyncCallbacks,
    AsyncErrorCallback,
    AsyncRequestCallback,
    AsyncResponseCallback,
    Callbacks,
    ErrorCallback,
    JsonValue,
    Methods,
    Request,
    RequestCallback,
    RequestContext,
    RequestTimings,
    Response,
    ResponseCallback,
    SyncCallback,
    _Stages,
)

__all__ = [
    "AsyncCallback",
    "AsyncCallbacks",
    "AsyncErrorCallback",
    "AsyncRequestCallback",
    "AsyncResponseCallback",
    "Callbacks",
    "ErrorCallback",
    "JsonValue",
    "Methods",
    "Request",
    "RequestCallback",
    "RequestContext",
    "RequestTimings",
    "Response",
    "ResponseCallback",
    "SyncCallback",
    "_Stages",
]
