from __future__ import annotations

from typing import TYPE_CHECKING, Any

from beekeepy._interface.abc.synchronous.session import Session as SessionInterface
from beekeepy._interface.synchronous.wallet import (
    UnlockedWallet,
    Wallet,
)

if TYPE_CHECKING:
    from beekeepy._interface.abc.synchronous.wallet import (
        UnlockedWallet as UnlockedWalletInterface,
    )
    from beekeepy._interface.abc.synchronous.wallet import (
        Wallet as WalletInterface,
    )
    from helpy import (
        Beekeeper as SynchronousBeekeeperHandle,
    )
    from schemas.apis.beekeeper_api import GetInfo


class Session(SessionInterface):
    def __init__(self, *args: Any, beekeeper: SynchronousBeekeeperHandle, **kwargs: Any) -> None:
        super().__init__(*args, **kwargs)
        self.__beekeeper = beekeeper

    def get_info(self) -> GetInfo:
        return self.__beekeeper.api.beekeeper.get_info()

    def create_wallet(self, *, name: str, password: str) -> UnlockedWalletInterface:
        self.__beekeeper.api.beekeeper.create(wallet_name=name, password=password)
        return self.__construct_unlocked_wallet(name)

    def open_wallet(self, *, name: str, password: str) -> UnlockedWalletInterface:
        return self.__construct_wallet(name=name).unlock(password=password)

    def close_session(self) -> None:
        self.__beekeeper.api.beekeeper.close_session()

    def lock_all(self) -> list[WalletInterface]:
        self.__beekeeper.api.beekeeper.lock_all()
        return self.wallets

    @property
    def wallets(self) -> list[WalletInterface]:
        return [
            self.__construct_wallet(name=wallet.name)
            for wallet in self.__beekeeper.api.beekeeper.list_wallets().wallets
        ]

    def __construct_unlocked_wallet(self, name: str) -> UnlockedWallet:
        return UnlockedWallet(name=name, beekeeper=self.__beekeeper)

    def __construct_wallet(self, name: str) -> Wallet:
        return Wallet(name=name, beekeeper=self.__beekeeper)
