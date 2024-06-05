from __future__ import annotations

from typing import TYPE_CHECKING, Any

from beekeepy._handle.beekeeper import AsyncBeekeeper as AsynchronousBeekeeperHandle
from beekeepy._handle.beekeeper import AsyncRemoteBeekeeper as AsynchronousRemoteBeekeeperHandle
from beekeepy._interface.abc.asynchronous.session import Session as SessionInterface
from beekeepy._interface.asynchronous.wallet import (
    UnlockedWallet,
    Wallet,
)
from beekeepy._interface.exceptions import NoWalletWithSuchNameError

if TYPE_CHECKING:
    from beekeepy._handle.callbacks_protocol import AsyncWalletLocked
    from beekeepy._interface.abc.asynchronous.wallet import (
        UnlockedWallet as UnlockedWalletInterface,
    )
    from beekeepy._interface.abc.asynchronous.wallet import (
        Wallet as WalletInterface,
    )
    from schemas.apis.beekeeper_api import GetInfo


class Session(SessionInterface):
    def __init__(self, *args: Any, beekeeper: AsynchronousBeekeeperHandle, **kwargs: Any) -> None:
        super().__init__(*args, **kwargs)
        self.__beekeeper = AsynchronousRemoteBeekeeperHandle(
            settings=beekeeper.settings.copy(update={"notification_endpoint": None}),
            logger=beekeeper.logger,
        )
        self.__beekeeper.run()

    async def get_info(self) -> GetInfo:
        return await self.__beekeeper.api.get_info()

    async def create_wallet(self, *, name: str, password: str) -> UnlockedWalletInterface:
        await self.__beekeeper.api.create(wallet_name=name, password=password)
        return self.__construct_unlocked_wallet(name)

    async def open_wallet(self, *, name: str) -> WalletInterface:
        with NoWalletWithSuchNameError(name):
            await self.__beekeeper.api.open(wallet_name=name)
        return self.__construct_wallet(name=name)

    async def close_session(self) -> None:
        if self.__beekeeper.is_session_token_set():
            await self.__beekeeper.api.close_session()
        self.__beekeeper.close()

    async def lock_all(self) -> list[WalletInterface]:
        await self.__beekeeper.api.lock_all()
        return await self.wallets

    async def set_timeout(self, seconds: int) -> None:
        await self.__beekeeper.api.set_timeout(seconds=seconds)

    def on_wallet_locked(self, callback: AsyncWalletLocked) -> None:
        self.__beekeeper.register_wallet_close_callback(callback)

    @property
    async def token(self) -> str:
        return await self.__beekeeper.session_token

    @property
    async def wallets(self) -> list[WalletInterface]:
        return [
            self.__construct_wallet(name=wallet.name) for wallet in (await self.__beekeeper.api.list_wallets()).wallets
        ]

    def __construct_unlocked_wallet(self, name: str) -> UnlockedWallet:
        return UnlockedWallet(name=name, beekeeper=self.__beekeeper)

    def __construct_wallet(self, name: str) -> WalletInterface:
        return Wallet(name=name, beekeeper=self.__beekeeper)
