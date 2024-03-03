from __future__ import annotations

from abc import ABC, abstractmethod
from types import TracebackType
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from beekeepy._interface.abc.asynchronous.session import Session
    from typing_extensions import Self


class Beekeeper(ABC):
    @abstractmethod
    async def create_session(self, *, salt: str | None = None) -> Session:
        ...

    def __enter__(self) -> Self:
        return self

    def __exit__(self, _: type[BaseException] | None, ex: BaseException | None, __: TracebackType | None) -> bool:
        return ex is None
