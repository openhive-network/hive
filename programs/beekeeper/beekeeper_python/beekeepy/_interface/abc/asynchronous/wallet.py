from __future__ import annotations

from abc import ABC, abstractmethod
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from types import TracebackType

    from typing_extensions import Self

    from schemas.fields.basic import PublicKey

class Wallet(ABC):
    @property
    @abstractmethod
    async def public_keys(self) -> list[PublicKey]:
        ...

    @property
    @abstractmethod
    async def unlocked(self) -> UnlockedWallet | None:
        ...

    @abstractmethod
    async def unlock(self, password: str) -> UnlockedWallet:
        ...


class UnlockedWallet(Wallet, ABC):
    @abstractmethod
    async def generate_key(self, *, salt: str | None = None) -> PublicKey:
        ...

    @abstractmethod
    async def import_key(self, *, private_key: str) -> PublicKey:
        ...

    @abstractmethod
    async def remove_key(self, *, key: PublicKey, confirmation_password: str) -> None:
        ...

    @abstractmethod
    async def lock(self) -> None:
        ...

    async def __enter__(self) -> Self:
        return self

    async def __exit__(
        self,
        _: type[BaseException] | None,
        exception: BaseException | None,
        traceback: TracebackType | None,
    ) -> None:
        await self.lock()
