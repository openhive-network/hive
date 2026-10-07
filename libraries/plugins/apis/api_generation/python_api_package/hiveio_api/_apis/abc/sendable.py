from __future__ import annotations

from abc import ABC, abstractmethod
from typing import TYPE_CHECKING, Literal

if TYPE_CHECKING:
    from hiveio_api._communication.abc.communicator_models import AsyncCallbacks, Callbacks, Methods
    from hiveio_api._communication.url import HttpUrl
    from schemas.jsonrpc import ExpectResultT, JSONRPCResult


class Sendable(ABC):
    @abstractmethod
    def is_testnet(self) -> bool:
        """Check if the current instance is connected to a testnet."""

    def _id_for_jsonrpc_request(self) -> int:
        """Returns id for jsonrpc request."""
        return 0


class SyncSendable(Sendable, ABC):
    @abstractmethod
    def _send(
        self,
        *,
        method: Methods,
        expected_type: type[ExpectResultT],
        serialization_type: Literal["hf26", "legacy"],
        data: str | None = None,
        url: HttpUrl | None = None,
        callbacks: Callbacks | None = None,
    ) -> JSONRPCResult[ExpectResultT]: ...


class AsyncSendable(Sendable, ABC):
    @abstractmethod
    async def _async_send(
        self,
        *,
        method: Methods,
        expected_type: type[ExpectResultT],
        serialization_type: Literal["hf26", "legacy"],
        data: str | None = None,
        url: HttpUrl | None = None,
        callbacks: AsyncCallbacks | None = None,
    ) -> JSONRPCResult[ExpectResultT]: ...
