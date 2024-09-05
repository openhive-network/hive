from __future__ import annotations

from abc import ABC, abstractmethod
from typing import TYPE_CHECKING

from helpy import ContextSync

if TYPE_CHECKING:
    from beekeepy._interface.abc.packed_object import Packed
    from beekeepy._interface.abc.synchronous.session import Session


class Beekeeper(ContextSync["Beekeeper"], ABC):
    @abstractmethod
    def create_session(self, *, salt: str | None = None) -> Session: ...

    @property
    @abstractmethod
    def session(self) -> Session: ...

    def _enter(self) -> Beekeeper:
        return self

    def _finally(self) -> None:
        self.delete()

    @abstractmethod
    def delete(self) -> None: ...

    @abstractmethod
    def pack(self) -> Packed[Beekeeper]: ...

    @abstractmethod
    def detach(self) -> None: ...
