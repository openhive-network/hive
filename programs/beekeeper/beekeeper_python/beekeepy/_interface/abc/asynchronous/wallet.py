from __future__ import annotations

from abc import ABC, abstractmethod
from typing import TYPE_CHECKING

from beekeepy._interface.common import ContainsWalletName
from helpy import ContextAsync

if TYPE_CHECKING:
    from schemas.fields.basic import PublicKey
    from schemas.fields.hex import Signature


class Wallet(ContainsWalletName, ABC):
    @property
    @abstractmethod
    async def public_keys(self) -> list[PublicKey]: ...

    @property
    @abstractmethod
    async def unlocked(self) -> UnlockedWallet | None: ...

    @abstractmethod
    async def unlock(self, password: str) -> UnlockedWallet: ...

    @property
    @abstractmethod
    def name(self) -> str: ...


class UnlockedWallet(Wallet, ContextAsync["UnlockedWallet"], ABC):
    @abstractmethod
    async def generate_key(self, *, salt: str | None = None) -> PublicKey: ...

    @abstractmethod
    async def import_key(self, *, private_key: str) -> PublicKey: ...

    @abstractmethod
    async def remove_key(self, *, key: PublicKey, confirmation_password: str) -> None: ...

    @abstractmethod
    async def lock(self) -> None: ...

    @abstractmethod
    async def sign_digest(self, *, sig_digest: str, key: PublicKey) -> Signature: ...

    @abstractmethod
    async def has_matching_private_key(self, *, key: PublicKey) -> bool: ...

    async def _aenter(self) -> UnlockedWallet:
        return self

    async def _afinally(self) -> None:
        await self.lock()
