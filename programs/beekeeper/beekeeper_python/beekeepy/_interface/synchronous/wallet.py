from __future__ import annotations

from typing import TYPE_CHECKING, Any

import wax
from beekeepy._interface.abc.synchronous.wallet import (
    UnlockedWallet as UnlockedWalletInterface,
)
from beekeepy._interface.abc.synchronous.wallet import (
    Wallet as WalletInterface,
)

if TYPE_CHECKING:
    from beekeepy._handle.beekeeper import Beekeeper
    from schemas.fields.basic import PublicKey
    from schemas.fields.hex import Signature


class Wallet(WalletInterface):
    def __init__(self, *args: Any, name: str, beekeeper: Beekeeper, **kwargs: Any) -> None:
        super().__init__(*args, **kwargs)
        self.__beekeeper = beekeeper
        self.__name = name

    @property
    def public_keys(self) -> list[PublicKey]:
        return [key.public_key for key in self.__beekeeper.api.get_public_keys().keys]

    @property
    def unlocked(self) -> UnlockedWallet | None:
        if self.__is_unlocked():
            return self.__construct_unlocked_wallet()
        return None

    def unlock(self, password: str) -> UnlockedWallet:
        if not self.__is_unlocked():
            self.__beekeeper.api.unlock(wallet_name=self.name, password=password)
        return self.__construct_unlocked_wallet()

    def __is_unlocked(self) -> bool:
        for wallet in self.__beekeeper.api.list_wallets().wallets:
            if wallet.name == self.__name:
                return wallet.unlocked
        return False

    def __construct_unlocked_wallet(self) -> UnlockedWallet:
        return UnlockedWallet(name=self.name, beekeeper=self.__beekeeper)

    @property
    def name(self) -> str:
        return self.__name


class UnlockedWallet(UnlockedWalletInterface, Wallet):
    def __init__(self, *args: Any, name: str, beekeeper: Beekeeper, **kwargs: Any) -> None:
        super().__init__(*args, name=name, beekeeper=beekeeper, **kwargs)
        self.__beekeeper = beekeeper

    def __get_wax_result(self, wax_result: wax.python_result) -> str:
        assert wax_result.exception_message is None or wax_result.exception_message == b""
        return wax_result.result.decode("ascii")

    def generate_key(self, *, salt: str | None = None) -> PublicKey:  # noqa: ARG002
        private_key = self.__get_wax_result(wax.generate_private_key())
        return self.import_key(private_key=private_key)

    def import_key(self, *, private_key: str) -> PublicKey:
        return self.__beekeeper.api.import_key(wallet_name=self.name, wif_key=private_key).public_key

    def remove_key(self, *, key: PublicKey, confirmation_password: str) -> None:
        self.__beekeeper.api.remove_key(wallet_name=self.name, password=confirmation_password, public_key=key)

    def lock(self) -> None:
        self.__beekeeper.api.lock(wallet_name=self.name)

    def sign_digest(self, *, sig_digest: str, key: PublicKey) -> Signature:
        return self.__beekeeper.api.sign_digest(sig_digest=sig_digest, public_key=key).signature
