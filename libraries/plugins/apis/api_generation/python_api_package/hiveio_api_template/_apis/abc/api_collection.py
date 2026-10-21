from __future__ import annotations

from typing import Generic

from hiveio_api._apis.abc.api import AbstractAsyncApi, AbstractSyncApi, HandleT
from hiveio_api._apis.abc.sendable import AsyncSendable, SyncSendable
from hiveio_api._communication.abc.communicator_models import AsyncCallbacks, Callbacks


class AbstractApiCollection(Generic[HandleT]):
    """Base class for Api Collections."""

    def __init__(self, owner: HandleT) -> None:
        self._owner = owner


class AbstractAsyncApiCollection(AbstractApiCollection[AsyncSendable]):
    """
    Base class for Async Api Collections.

    APIs assigned to attributes of the collection call callbacks of the collection before their own ones.
    """

    def __init__(self, owner: AsyncSendable) -> None:
        super().__init__(owner)
        self.__callbacks = AsyncCallbacks()

    @property
    def callbacks(self) -> AsyncCallbacks:
        """Callbacks of requests of all APIs of the collection - called before callbacks of the API and endpoint."""
        return self.__callbacks

    def __setattr__(self, name: str, value: object) -> None:
        super().__setattr__(name, value)
        if isinstance(value, AbstractAsyncApi):
            value._attach_to_collection(self)  # noqa: SLF001  # the collection is the parent of its APIs


class AbstractSyncApiCollection(AbstractApiCollection[SyncSendable]):
    """
    Base class for Sync Api Collections.

    APIs assigned to attributes of the collection call callbacks of the collection before their own ones.
    """

    def __init__(self, owner: SyncSendable) -> None:
        super().__init__(owner)
        self.__callbacks = Callbacks()

    @property
    def callbacks(self) -> Callbacks:
        """Callbacks of requests of all APIs of the collection - called before callbacks of the API and endpoint."""
        return self.__callbacks

    def __setattr__(self, name: str, value: object) -> None:
        super().__setattr__(name, value)
        if isinstance(value, AbstractSyncApi):
            value._attach_to_collection(self)  # noqa: SLF001  # the collection is the parent of its APIs
