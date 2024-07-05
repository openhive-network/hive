from __future__ import annotations

from abc import ABC, abstractmethod
from typing import TYPE_CHECKING

from beekeepy._interface.common import ContainsWalletName
from helpy import ContextSync
from schemas.fields.basic import PublicKey

if TYPE_CHECKING:
    from schemas.fields.hex import Signature


class Wallet(ContainsWalletName, ABC):
    @property
    @abstractmethod
    def public_keys(self) -> list[PublicKey]: ...

    @property
    @abstractmethod
    def unlocked(self) -> UnlockedWallet | None: ...

    @abstractmethod
    def unlock(self, password: str) -> UnlockedWallet: ...

    @property
    @abstractmethod
    def name(self) -> str: ...


class UnlockedWallet(Wallet, ContextSync["UnlockedWallet"], ABC):
    @abstractmethod
    def generate_key(self, *, salt: str | None = None) -> PublicKey: ...

    @abstractmethod
    def import_key(self, *, private_key: str) -> PublicKey: ...

    @abstractmethod
    def import_keys(self, *, private_keys: list[str]) -> list[PublicKey]: ...

    @abstractmethod
    def remove_key(self, *, key: PublicKey, confirmation_password: str) -> None: ...

    @abstractmethod
    def lock(self) -> None: ...

    @abstractmethod
    def sign_digest(self, *, sig_digest: str, key: PublicKey) -> Signature: ...

    @abstractmethod
    def has_matching_private_key(self, *, key: PublicKey) -> bool: ...

    @abstractmethod
    def encrypt_data(self, *, from_key: PublicKey, to_key: PublicKey, content: str, nonce: int = 0) -> str:
        ...

    @abstractmethod
    def decrypt_data(self, *, from_key: PublicKey, to_key: PublicKey, content: str) -> str:
        ...

    def _enter(self) -> UnlockedWallet:
        return self

    def _finally(self) -> None:
        self.lock()

    def __contains__(self, obj: object) -> bool:
        if isinstance(obj, str):
            return self.has_matching_private_key(key=PublicKey(obj))
        raise TypeError(f"Object `{obj}` is not str which can't be check for matchin private key in wallet.")
