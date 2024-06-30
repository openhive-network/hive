from __future__ import annotations

from abc import ABC, abstractmethod
from typing import TYPE_CHECKING, overload

from typing_extensions import Self

from helpy import ContextSync

if TYPE_CHECKING:

    from beekeepy._handle.callbacks_protocol import SyncWalletLocked
    from beekeepy._interface.abc.synchronous.wallet import UnlockedWallet, Wallet
    from schemas.apis.beekeeper_api import GetInfo
    from schemas.fields.basic import PublicKey


class Session(ContextSync[Self], ABC):
    @abstractmethod
    def get_info(self) -> GetInfo:
        ...

    @overload
    @abstractmethod
    def create_wallet(self, *, name: str, password: str) -> UnlockedWallet:
        ...

    @overload
    @abstractmethod
    def create_wallet(self, *, name: str, password: None = None) -> tuple[UnlockedWallet, str]:
        ...

    @abstractmethod
    def create_wallet(self, *, name: str, password: str | None = None) -> UnlockedWallet | tuple[UnlockedWallet, str]:
        ...

    @abstractmethod
    def open_wallet(self, *, name: str) -> Wallet:
        ...

    @abstractmethod
    def close_session(self) -> None:
        ...

    @abstractmethod
    def lock_all(self) -> list[Wallet]:
        ...

    @abstractmethod
    def set_timeout(self, seconds: int) -> None:
        ...

    @property
    @abstractmethod
    def wallets(self) -> list[Wallet]:
        ...

    @property
    @abstractmethod
    def created_wallets(self) -> list[Wallet]:
        ...

    @property
    def opened_wallets(self) -> list[Wallet]:
        return [wallet for wallet in self.wallets if wallet.unlocked is not None]

    @property
    @abstractmethod
    def token(self) -> str:
        ...

    @property
    def public_keys(self) -> list[PublicKey]:
        ...

    @abstractmethod
    def on_wallet_locked(self, callback: SyncWalletLocked) -> None:
        ...

    def _enter(self) -> Self:
        return super()._enter()

    def _finally(self) -> None:
        self.close_session()
