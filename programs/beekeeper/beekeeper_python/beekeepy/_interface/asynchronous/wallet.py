from __future__ import annotations

from typing import TYPE_CHECKING, Any

import wax
from beekeepy._interface.abc.asynchronous.wallet import (
    UnlockedWallet as UnlockedWalletInterface,
)
from beekeepy._interface.abc.asynchronous.wallet import (
    Wallet as WalletInterface,
)

if TYPE_CHECKING:
    from beekeepy._handle.beekeeper import AsyncBeekeeper
    from schemas.fields.basic import PublicKey
    from schemas.fields.hex import Signature


class Wallet(WalletInterface):
    def __init__(self, *args: Any, name: str, beekeeper: AsyncBeekeeper, **kwargs: Any) -> None:
        super().__init__(*args, **kwargs)
        self.__beekeeper = beekeeper
        self.__name = name

    @property
    async def public_keys(self) -> list[PublicKey]:
        return [key.public_key for key in (await self.__beekeeper.api.beekeeper.get_public_keys()).keys]

    @property
    async def unlocked(self) -> UnlockedWallet | None:
        if await self.__is_unlocked():
            return self.__construct_unlocked_wallet()
        return None

    async def unlock(self, password: str) -> UnlockedWallet:
        if not (await self.__is_unlocked()):
            await self.__beekeeper.api.beekeeper.unlock(wallet_name=self.name, password=password)
        return self.__construct_unlocked_wallet()

    async def __is_unlocked(self) -> bool:
        for wallet in (await self.__beekeeper.api.beekeeper.list_wallets()).wallets:
            if wallet.name == self.__name:
                return wallet.unlocked
        return False

    def __construct_unlocked_wallet(self) -> UnlockedWallet:
        return UnlockedWallet(name=self.name, beekeeper=self.__beekeeper)

    @property
    def name(self) -> str:
        return self.__name


class UnlockedWallet(UnlockedWalletInterface, Wallet):
    def __init__(self, *args: Any, name: str, beekeeper: AsyncBeekeeper, **kwargs: Any) -> None:
        super().__init__(*args, name=name, beekeeper=beekeeper, **kwargs)
        self.__beekeeper = beekeeper

    def __get_wax_result(self, wax_result: wax.python_result) -> str:
        assert wax_result.exception_message is None or wax_result.exception_message == b""
        return wax_result.result.decode("ascii")

    async def generate_key(self, *, salt: str | None = None) -> PublicKey:  # noqa: ARG002
        private_key = self.__get_wax_result(wax.generate_private_key())
        return await self.import_key(private_key=private_key)

    async def import_key(self, *, private_key: str) -> PublicKey:
        return (await self.__beekeeper.api.beekeeper.import_key(wallet_name=self.name, wif_key=private_key)).public_key

    async def remove_key(self, *, key: PublicKey, confirmation_password: str) -> None:
        await self.__beekeeper.api.beekeeper.remove_key(
            wallet_name=self.name, password=confirmation_password, public_key=key
        )

    async def lock(self) -> None:
        await self.__beekeeper.api.beekeeper.lock(wallet_name=self.name)

    async def sign_digest(self, *, sig_digest: str, key: PublicKey) -> Signature:
        return (await self.__beekeeper.api.beekeeper.sign_digest(sig_digest=sig_digest, public_key=key)).signature
