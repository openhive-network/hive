from __future__ import annotations

from abc import ABC, abstractmethod
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from types import TracebackType

    from typing_extensions import Self

    from beekeepy._interface.abc.synchronous.wallet import UnlockedWallet, Wallet
    from schemas.apis.beekeeper_api import GetInfo


class Session(ABC):
    @abstractmethod
    def get_info(self) -> GetInfo:
        ...

    @abstractmethod
    def create_wallet(self, *, name: str, password: str) -> UnlockedWallet:
        ...

    @abstractmethod
    def open_wallet(self, *, name: str, password: str) -> UnlockedWallet:
        ...

    @abstractmethod
    def close_session(self) -> None:
        ...

    @abstractmethod
    def lock_all(self) -> list[Wallet]:
        ...

    @property
    @abstractmethod
    def wallets(self) -> list[Wallet]:
        ...

    def __enter__(self) -> Self:
        return self

    def __exit__(
        self,
        _: type[BaseException] | None,
        exception: BaseException | None,
        traceback: TracebackType | None,
    ) -> bool:
        self.close_session()
        return exception is None
