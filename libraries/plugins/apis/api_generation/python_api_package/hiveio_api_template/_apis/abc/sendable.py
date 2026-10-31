from __future__ import annotations

from abc import ABC, abstractmethod
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from hiveio_api._communication.abc.communicator_models import AsyncCallbacks, Callbacks, Methods
    from hiveio_api._communication.jsonrpc import ExpectResultT, JSONRPCResult
    from hiveio_api._communication.url import HttpUrl


class Sendable(ABC):
    """Owner of APIs sending their requests (e.g. a handle of a node) - see `SyncSendable` and `AsyncSendable`."""

    @abstractmethod
    def is_testnet(self) -> bool:
        """Check if the current instance is connected to a testnet."""

    def _id_for_jsonrpc_request(self) -> int:
        """Returns id for jsonrpc request."""
        return 0


class SyncSendable(Sendable, ABC):
    """Owner of synchronous APIs (`AbstractSyncApi`) - sends their requests with `_send`."""

    @abstractmethod
    def _send(
        self,
        *,
        method: Methods,
        expected_type: type[ExpectResultT],
        data: str | None = None,
        url: HttpUrl | None = None,
        callbacks: Callbacks | None = None,
    ) -> JSONRPCResult[ExpectResultT]: ...


class AsyncSendable(Sendable, ABC):
    """Owner of asynchronous APIs (`AbstractAsyncApi`) - sends their requests with `_async_send`."""

    @abstractmethod
    async def _async_send(
        self,
        *,
        method: Methods,
        expected_type: type[ExpectResultT],
        data: str | None = None,
        url: HttpUrl | None = None,
        callbacks: AsyncCallbacks | None = None,
    ) -> JSONRPCResult[ExpectResultT]: ...
