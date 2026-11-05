"""
Deprecated - the objects of `hiveio_api_client._apis.abc.rest_api_definition_helpers` under their former path in hiveio_api,
kept for packages importing it. Use `hiveio_api_client._apis.abc.rest_api_definition_helpers` instead.
"""

from __future__ import annotations

from hiveio_api_client._apis.abc.rest_api_definition_helpers import (
    AsyncApiT,
    AsyncRestApiDefinitionHelper,
    P,
    R,
    SyncApiT,
    SyncRestApiDefinitionHelper,
)

__all__ = [
    "AsyncApiT",
    "AsyncRestApiDefinitionHelper",
    "P",
    "R",
    "SyncApiT",
    "SyncRestApiDefinitionHelper",
]
