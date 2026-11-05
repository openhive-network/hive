"""
Deprecated - the objects of `hiveio_api_client._apis.abc.sendable` under their former path in hiveio_api,
kept for packages importing it. Use `hiveio_api.client` (`hiveio_api_client` for the names it does not export) instead.
"""

from __future__ import annotations

from hiveio_api_client._apis.abc.sendable import (
    AsyncSendable,
    Sendable,
    SyncSendable,
)

__all__ = [
    "AsyncSendable",
    "Sendable",
    "SyncSendable",
]
