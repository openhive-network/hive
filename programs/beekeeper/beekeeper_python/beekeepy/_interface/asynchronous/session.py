from __future__ import annotations

from typing import TYPE_CHECKING, Any

from beekeepy._interface.validators import validate_wallet_name, validate_seconds, validate_public_keys
from beekeepy._handle.beekeeper import AsyncRemoteBeekeeper as AsynchronousRemoteBeekeeperHandle
from beekeepy._interface.abc.asynchronous.session import Session as SessionInterface
from beekeepy._interface.asynchronous.wallet import (
    UnlockedWallet,
    Wallet,
)
from beekeepy._interface.exceptions import (
    InvalidWalletError,
    NoWalletWithSuchNameError,
    WalletWithSuchNameAlreadyExistsError,
)
from beekeepy._interface.validators import validate_public_keys, validate_seconds, validate_wallet_name

if TYPE_CHECKING:
    from beekeepy._interface.abc.asynchronous.wallet import (
        UnlockedWallet as UnlockedWalletInterface,
    )
    from beekeepy._interface.abc.asynchronous.wallet import (
        Wallet as WalletInterface,
    )
    from schemas.apis.beekeeper_api import GetInfo
    from schemas.fields.basic import PublicKey


class Session(SessionInterface):
    def __init__(self, *args: Any, beekeeper: AsynchronousRemoteBeekeeperHandle, **kwargs: Any) -> None:
        super().__init__(*args, **kwargs)
        self.__beekeeper = AsynchronousRemoteBeekeeperHandle(
            settings=beekeeper.settings,
        )
        self.__beekeeper.run()

    async def get_info(self) -> GetInfo:
        return await self.__beekeeper.api.get_info()

    async def create_wallet(  # type: ignore[override]
        self, *, name: str, password: str | None = None
        validate_wallet_name(wallet_name=name)
        with WalletWithSuchNameAlreadyExistsError(wallet_name=name), InvalidWalletError(wallet_name=name):
            create_result = await self.__beekeeper.api.create(wallet_name=name, password=password)
        wallet = self.__construct_unlocked_wallet(name)
        return wallet if password is not None else (wallet, create_result.password)

    async def open_wallet(self, *, name: str) -> WalletInterface:
        validate_wallet_name(wallet_name=name)
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
        validate_seconds(time=seconds)
        await self.__beekeeper.api.set_timeout(seconds=seconds)

    @property
    async def public_keys(self) -> list[PublicKey]:
        return [item.public_key for item in (await self.__beekeeper.api.get_public_keys()).keys]

    @property
    async def token(self) -> str:
        return await self.__beekeeper.session_token

    @property
    async def wallets(self) -> list[WalletInterface]:
        return [
            self.__construct_wallet(name=wallet.name) for wallet in (await self.__beekeeper.api.list_wallets()).wallets
        ]

    @property
    async def created_wallets(self) -> list[WalletInterface]:
        return [
            self.__construct_wallet(name=wallet.name)
            for wallet in (await self.__beekeeper.api.list_created_wallets()).wallets
        ]

    def __construct_unlocked_wallet(self, name: str) -> UnlockedWallet:
        return UnlockedWallet(name=name, beekeeper=self.__beekeeper)

    def __construct_wallet(self, name: str) -> WalletInterface:
        return Wallet(name=name, beekeeper=self.__beekeeper)
