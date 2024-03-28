from __future__ import annotations

from typing import TYPE_CHECKING, Any

from beekeepy._interface.abc.asynchronous.session import Session as SessionInterface
from beekeepy._interface.asynchronous.wallet import (
    UnlockedWallet,
    Wallet,
)
from helpy import (
    AsyncBeekeeper as AsynchronousBeekeeperHandle,
)
from helpy import (
    HttpUrl,
)

if TYPE_CHECKING:
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
        self.__beekeeper = AsynchronousBeekeeperHandle(
            settings=beekeeper.settings.copy(update={"notification_endpoint": HttpUrl("127.0.0.1:0")})
        )

    async def get_info(self) -> GetInfo:
        return await self.__beekeeper.api.beekeeper.get_info()

    async def create_wallet(self, *, name: str, password: str) -> UnlockedWalletInterface:
        await self.__beekeeper.api.beekeeper.create(wallet_name=name, password=password)
        return self.__construct_unlocked_wallet(name)

    async def open_wallet(self, *, name: str, password: str) -> UnlockedWalletInterface:
        return await self.__construct_wallet(name=name).unlock(password=password)

    async def close_session(self) -> None:
        await self.__beekeeper.api.beekeeper.close_session()

    async def lock_all(self) -> list[WalletInterface]:
        await self.__beekeeper.api.beekeeper.lock_all()
        return await self.wallets

    @property
    def token(self) -> str:
        return self.__beekeeper.session_token

    @property
    async def wallets(self) -> list[WalletInterface]:
        return [
            self.__construct_wallet(name=wallet.name)
            for wallet in (await self.__beekeeper.api.beekeeper.list_wallets()).wallets
        ]

    def __construct_unlocked_wallet(self, name: str) -> UnlockedWallet:
        return UnlockedWallet(name=name, beekeeper=self.__beekeeper)

    def __construct_wallet(self, name: str) -> Wallet:
        return Wallet(name=name, beekeeper=self.__beekeeper)
