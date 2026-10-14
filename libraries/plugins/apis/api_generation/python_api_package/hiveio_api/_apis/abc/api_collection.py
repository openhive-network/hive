from __future__ import annotations

from typing import Generic

from hiveio_api._apis.abc.api import HandleT
from hiveio_api._apis.abc.sendable import AsyncSendable, SyncSendable


class AbstractApiCollection(Generic[HandleT]):
    """Base class for Api Collections."""

    def __init__(self, owner: HandleT) -> None:
        self._owner = owner


class AbstractAsyncApiCollection(AbstractApiCollection[AsyncSendable]):
    """Base class for Async Api Collections."""

    def __init__(self, owner: AsyncSendable) -> None:
        super().__init__(owner)


class AbstractSyncApiCollection(AbstractApiCollection[SyncSendable]):
    """Base class for Sync Api Collections."""

    def __init__(self, owner: SyncSendable) -> None:
        super().__init__(owner)
