from __future__ import annotations

from abc import ABC, abstractmethod
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from types import TracebackType

    from typing_extensions import Self

    from beekeepy._interface.abc.packed_object import Packed
    from beekeepy._interface.abc.synchronous.session import Session


class Beekeeper(ABC):
    @abstractmethod
    def create_session(self, *, salt: str | None = None) -> Session:
        ...

    def __enter__(self) -> Self:
        return self

    def __exit__(self, _: type[BaseException] | None, ex: BaseException | None, __: TracebackType | None) -> bool:
        self.delete()
        return ex is None

    @abstractmethod
    def delete(self) -> None:
        ...

    @abstractmethod
    def pack(self) -> Packed[Beekeeper]:
        ...
