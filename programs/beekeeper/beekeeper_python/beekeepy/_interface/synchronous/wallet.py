from __future__ import annotations

from typing import TYPE_CHECKING

from beekeepy._handle.beekeeper import SyncRemoteBeekeeper
from beekeepy._handle.callbacks_protocol import SyncWalletLocked
from beekeepy._interface.abc.synchronous.wallet import (
    UnlockedWallet as UnlockedWalletInterface,
)
from beekeepy._interface.abc.synchronous.wallet import (
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


class Wallet(WalletCommons[SyncRemoteBeekeeper, SyncWalletLocked], WalletInterface):
    @property
    def public_keys(self) -> list[PublicKey]:
        return [
            key.public_key
            for key in self._beekeeper.api.get_public_keys(wallet_name=self.name, token=self.session_token).keys
        ]

    @property
    def unlocked(self) -> UnlockedWallet | None:
        if self.__is_unlocked():
            return self.__construct_unlocked_wallet()
        return None

    def unlock(self, password: str) -> UnlockedWallet:
        if not self.__is_unlocked():
            self._beekeeper.api.unlock(wallet_name=self.name, password=password, token=self.session_token)
        return self.__construct_unlocked_wallet()

    def __is_unlocked(self) -> bool:
        for wallet in self._beekeeper.api.list_wallets(token=self.session_token).wallets:
            if wallet.name == self.name:
                self._last_lock_state = wallet.unlocked
                return self._last_lock_state
        self._last_lock_state = False
        return self._last_lock_state

    def __construct_unlocked_wallet(self) -> UnlockedWallet:
        wallet = UnlockedWallet(name=self.name, beekeeper=self._beekeeper, session_token=self.session_token)
        wallet._last_lock_state = False
        return wallet


class UnlockedWallet(UnlockedWalletInterface, Wallet):
    wallet_unlocked = WalletCommons.check_wallet

    @wallet_unlocked
    def generate_key(self, *, salt: str | None = None) -> PublicKey:  # noqa: ARG002
        return self.import_key(private_key=wax.generate_private_key())

    @wallet_unlocked
    def import_key(self, *, private_key: str) -> PublicKey:
        validate_private_keys(private_key=private_key)
        with InvalidPrivateKeyError(wif=private_key):
            return self._beekeeper.api.import_key(
                wallet_name=self.name, wif_key=private_key, token=self.session_token
            ).public_key

    @wallet_unlocked
    def remove_key(self, *, key: str, confirmation_password: str) -> None:
        validate_public_keys(key=key)
        with RemovingNotExistingKeyError(public_key=key), MissingSTMPrefixError(public_key=key), InvalidPublicKeyError(
            public_key=key
        ):
            self._beekeeper.api.remove_key(
                wallet_name=self.name, password=confirmation_password, public_key=key, token=self.session_token
            )

    @wallet_unlocked
    def lock(self) -> None:
        self._beekeeper.api.lock(wallet_name=self.name, token=self.session_token)

    @wallet_unlocked
    def sign_digest(self, *, sig_digest: str, key: str) -> Signature:
        validate_public_keys(key=key)
        with MissingSTMPrefixError(public_key=key), InvalidPublicKeyError(public_key=key):
            return self._beekeeper.api.sign_digest(
                sig_digest=sig_digest, public_key=key, wallet_name=self.name, token=self.session_token
            ).signature

    @wallet_unlocked
    def has_matching_private_key(self, *, key: str) -> bool:
        validate_public_keys(key=key)
        return self._beekeeper.api.has_matching_private_key(
            wallet_name=self.name, public_key=key, token=self.session_token
        ).exists

    @property
    def lock_time(self) -> datetime:
        return self._beekeeper.api.get_info(token=self.session_token).timeout_time
