"""
Deprecated - the objects of `hiveio_api_client._apis.abc.api` under their former path in hiveio_api,
kept for packages importing it. Use `hiveio_api.client` (`hiveio_api_client` for the names it does not export) instead.
"""

from __future__ import annotations

from hiveio_api._value_codec import _validating_type_to_builtins
from hiveio_api_client._apis.abc.api import (
    AbstractApi,
    AbstractAsyncApi,
    AbstractSyncApi,
    ApiArgumentSerialization,
    ApiArgumentsToSerialize,
    AsyncApiT,
    HandleT,
    P,
    R,
    RegisteredApisT,
    SyncApiT,
    _ApiCallbacks,
    _convert_pascal_case_to_sneak_case,
)

__all__ = [
    "AbstractApi",
    "AbstractAsyncApi",
    "AbstractSyncApi",
    "ApiArgumentSerialization",
    "ApiArgumentsToSerialize",
    "AsyncApiT",
    "HandleT",
    "P",
    "R",
    "RegisteredApisT",
    "SyncApiT",
    "_ApiCallbacks",
    "_convert_pascal_case_to_sneak_case",
    "_validating_type_to_builtins",
]
