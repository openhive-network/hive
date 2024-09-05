from __future__ import annotations

from abc import ABC, abstractmethod
from typing import TYPE_CHECKING

from helpy import ContextAsync

if TYPE_CHECKING:
    from beekeepy._interface.abc.asynchronous.session import Session
    from beekeepy._interface.abc.packed_object import Packed


class Beekeeper(ContextAsync["Beekeeper"], ABC):
    @abstractmethod
    async def create_session(self, *, salt: str | None = None) -> Session: ...

    @property
    @abstractmethod
    async def session(self) -> Session: ...

    async def _aenter(self) -> Beekeeper:
        return self

    async def _afinally(self) -> None:
        self.delete()

    @abstractmethod
    def delete(self) -> None: ...

    @abstractmethod
    def pack(self) -> Packed[Beekeeper]: ...

    @abstractmethod
    def detach(self) -> None: ...
