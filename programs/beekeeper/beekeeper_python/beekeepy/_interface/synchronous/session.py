from __future__ import annotations

from typing import TYPE_CHECKING, Any

from beekeepy._interface.validators import validate_wallet_name, validate_seconds, validate_public_keys
from beekeepy._handle.beekeeper import SyncRemoteBeekeeper
from beekeepy._interface.abc.synchronous.session import Session as SessionInterface
from beekeepy._interface.exceptions import (
    InvalidWalletError,
    NoWalletWithSuchNameError,
    WalletWithSuchNameAlreadyExistsError,
)
from beekeepy._interface.synchronous.wallet import (
    UnlockedWallet,
    Wallet,
)
from beekeepy._interface.validators import validate_public_keys, validate_seconds, validate_wallet_name

if TYPE_CHECKING:
    from beekeepy._interface.abc.synchronous.wallet import (
        UnlockedWallet as UnlockedWalletInterface,
    )
    from beekeepy._interface.abc.synchronous.wallet import (
        Wallet as WalletInterface,
    )
    from schemas.apis.beekeeper_api import GetInfo
    from schemas.fields.basic import PublicKey
    from schemas.fields.hex import Signature


class Session(SessionInterface):
    def __init__(self, *args: Any, beekeeper: SyncRemoteBeekeeper, **kwargs: Any) -> None:
        super().__init__(*args, **kwargs)
        self.__beekeeper = SyncRemoteBeekeeper(
            settings=beekeeper.settings,
        )
        self.__beekeeper.run()

    def get_info(self) -> GetInfo:
        return self.__beekeeper.api.get_info()

    def create_wallet(  # type: ignore[override]
        self, *, name: str, password: str | None = None
        validate_wallet_name(wallet_name=name)
        with WalletWithSuchNameAlreadyExistsError(wallet_name=name), InvalidWalletError(wallet_name=name):
            create_result = self.__beekeeper.api.create(wallet_name=name, password=password)
        wallet = self.__construct_unlocked_wallet(name)
        return wallet if password is not None else (wallet, create_result.password)

    def open_wallet(self, *, name: str) -> WalletInterface:
        validate_wallet_name(wallet_name=name)
        with NoWalletWithSuchNameError(name):
            self.__beekeeper.api.open(wallet_name=name)
        return self.__construct_wallet(name=name)

    def close_session(self) -> None:
        if self.__beekeeper.is_session_token_set():
            self.__beekeeper.api.close_session()
        self.__beekeeper.close()

    def lock_all(self) -> list[WalletInterface]:
        self.__beekeeper.api.lock_all()
        return self.wallets

    def set_timeout(self, seconds: int) -> None:
        validate_seconds(time=seconds)
        self.__beekeeper.api.set_timeout(seconds=seconds)

    def sign_digest(self, *, sig_digest: str, key: str) -> Signature:
        validate_public_keys(key=key)
        return self.__beekeeper.api.sign_digest(sig_digest=sig_digest, public_key=key).signature

    @property
    def token(self) -> str:
        return self.__beekeeper.session_token

    @property
    def wallets(self) -> list[WalletInterface]:
        return [self.__construct_wallet(name=wallet.name) for wallet in self.__beekeeper.api.list_wallets().wallets]

    @property
    def wallets_created(self) -> list[WalletInterface]:
        return [
            self.__construct_wallet(name=wallet.name) for wallet in self.__beekeeper.api.list_created_wallets().wallets
        ]

    @property
    def public_keys(self) -> list[PublicKey]:
        return [key.public_key for key in self.__beekeeper.api.get_public_keys().keys]

    def __construct_unlocked_wallet(self, name: str) -> UnlockedWallet:
        return UnlockedWallet(name=name, beekeeper=self.__beekeeper)

    def __construct_wallet(self, name: str) -> Wallet:
        return Wallet(name=name, beekeeper=self.__beekeeper)
