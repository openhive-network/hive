from __future__ import annotations

from abc import ABC, abstractmethod
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from types import TracebackType

    from typing_extensions import Self

    from beekeepy._handle.callbacks_protocol import AsyncWalletLocked
    from beekeepy._interface.abc.asynchronous.wallet import UnlockedWallet, Wallet
    from schemas.apis.beekeeper_api import GetInfo


class Session(ABC):
    @abstractmethod
    async def get_info(self) -> GetInfo:
        ...

    @abstractmethod
    async def create_wallet(self, *, name: str, password: str) -> UnlockedWallet:
        ...

    @abstractmethod
    async def open_wallet(self, *, name: str) -> Wallet:
        ...

    @abstractmethod
    async def close_session(self) -> None:
        ...

    @abstractmethod
    async def lock_all(self) -> list[Wallet]:
        ...

    @abstractmethod
    async def set_timeout(self, seconds: int) -> None:
        ...

    @property
    @abstractmethod
    async def wallets(self) -> list[Wallet]:
        ...

    @property
    @abstractmethod
    async def token(self) -> str:
        ...

    @abstractmethod
    def on_wallet_locked(self, callback: AsyncWalletLocked) -> None:
        ...

    async def __enter__(self) -> Self:
        return self

    async def __exit__(
        self,
        _: type[BaseException] | None,
        exception: BaseException | None,
        traceback: TracebackType | None,
    ) -> bool:
        await self.close_session()
        return exception is None
