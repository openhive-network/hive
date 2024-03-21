from __future__ import annotations

from abc import ABC, abstractmethod
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from types import TracebackType

    from typing_extensions import Self

    from beekeepy._interface.abc.asynchronous.session import Session


class Beekeeper(ABC):
    @abstractmethod
    async def create_session(self, *, salt: str | None = None) -> Session: ...

    def __enter__(self) -> Self:
        return self

    def __exit__(self, _: type[BaseException] | None, ex: BaseException | None, __: TracebackType | None) -> bool:
        self.delete()
        return ex is None

    def delete(self) -> None:
        pass
