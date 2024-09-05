from __future__ import annotations

from typing import TYPE_CHECKING

from beekeepy._handle.beekeeper import AsyncRemoteBeekeeper
from beekeepy._handle.callbacks_protocol import AsyncWalletLocked
from beekeepy._interface.abc.asynchronous.wallet import (
    UnlockedWallet as UnlockedWalletInterface,
)
from beekeepy._interface.abc.asynchronous.wallet import (
    Wallet as WalletInterface,
)
from beekeepy._interface.common import WalletCommons
from beekeepy._interface.exceptions import (
    InvalidPrivateKeyError,
    InvalidPublicKeyError,
    MissingSTMPrefixError,
    RemovingNotExistingKeyError,
)
from beekeepy._interface.validators import validate_private_keys, validate_public_keys
from helpy import wax

if TYPE_CHECKING:
    from datetime import datetime

    from schemas.fields.basic import PublicKey
    from schemas.fields.hex import Signature


class Wallet(WalletCommons[AsyncRemoteBeekeeper, AsyncWalletLocked], WalletInterface):
    @property
    async def public_keys(self) -> list[PublicKey]:
        return [key.public_key for key in (await self._beekeeper.api.get_public_keys()).keys]

    @property
    async def unlocked(self) -> UnlockedWallet | None:
        if await self.__is_unlocked():
            return self.__construct_unlocked_wallet()
        return None

    async def unlock(self, password: str) -> UnlockedWallet:
        if not (await self.__is_unlocked()):
            await self._beekeeper.api.unlock(wallet_name=self.name, password=password)
        return self.__construct_unlocked_wallet()

    async def __is_unlocked(self) -> bool:
        for wallet in (await self._beekeeper.api.list_wallets()).wallets:
            if wallet.name == self.name:
                self._last_lock_state = wallet.unlocked
                return self._last_lock_state
        self._last_lock_state = False
        return self._last_lock_state

    def __construct_unlocked_wallet(self) -> UnlockedWallet:
        wallet = UnlockedWallet(name=self.name, beekeeper=self._beekeeper)
        wallet._last_lock_state = False
        return wallet


class UnlockedWallet(Wallet, UnlockedWalletInterface):
    wallet_unlocked = WalletCommons.check_wallet

    @wallet_unlocked
    async def generate_key(self, *, salt: str | None = None) -> PublicKey:  # noqa: ARG002
        return await self.import_key(private_key=wax.generate_private_key())

    @wallet_unlocked
    async def import_key(self, *, private_key: str) -> PublicKey:
        validate_private_keys(private_key=private_key)
        with InvalidPrivateKeyError(wif=private_key):
            return (await self._beekeeper.api.import_key(wallet_name=self.name, wif_key=private_key)).public_key

    @wallet_unlocked
    async def remove_key(self, *, key: str, confirmation_password: str) -> None:
        validate_public_keys(key=key)
        with RemovingNotExistingKeyError(public_key=key), MissingSTMPrefixError(public_key=key), InvalidPublicKeyError(public_key=key):
            await self._beekeeper.api.remove_key(wallet_name=self.name, password=confirmation_password, public_key=key)

    @wallet_unlocked
    async def lock(self) -> None:
        await self._beekeeper.api.lock(wallet_name=self.name)

    @wallet_unlocked
    async def sign_digest(self, *, sig_digest: str, key: str) -> Signature:
        validate_public_keys(key=key)
        with MissingSTMPrefixError(public_key=key), InvalidPublicKeyError(public_key=key):
            return (await self._beekeeper.api.sign_digest(sig_digest=sig_digest, public_key=key, wallet_name=self.name)).signature

    @wallet_unlocked
    async def has_matching_private_key(self, *, key: str) -> bool:
        validate_public_keys(key=key)
        return (await self._beekeeper.api.has_matching_private_key(wallet_name=self.name, public_key=key)).exists

    @property
    async def lock_time(self) -> datetime:
        return (await self._beekeeper.api.get_info()).timeout_time
