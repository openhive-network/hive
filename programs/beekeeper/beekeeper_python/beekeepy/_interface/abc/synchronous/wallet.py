from __future__ import annotations

from abc import ABC, abstractmethod
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from types import TracebackType

    from typing_extensions import Self

    from schemas.fields.basic import PublicKey
    from schemas.fields.hex import Signature


class Wallet(ABC):
    @property
    @abstractmethod
    def public_keys(self) -> list[PublicKey]:
        ...

    @property
    @abstractmethod
    def unlocked(self) -> UnlockedWallet | None:
        ...

    @abstractmethod
    def unlock(self, password: str) -> UnlockedWallet:
        ...

    @property
    @abstractmethod
    def name(self) -> str:
        ...


class UnlockedWallet(Wallet, ABC):
    @abstractmethod
    def generate_key(self, *, salt: str | None = None) -> PublicKey:
        ...

    @abstractmethod
    def import_key(self, *, private_key: str) -> PublicKey:
        ...

    @abstractmethod
    def remove_key(self, *, key: PublicKey, confirmation_password: str) -> None:
        ...

    @abstractmethod
    def lock(self) -> None:
        ...

    @abstractmethod
    def sign_digest(self, *, sig_digest: str, key: PublicKey) -> Signature:
        ...

    @abstractmethod
    def encrypt_data(self, *, from_key: PublicKey, to_key: PublicKey, content: str, nonce: int = 0) -> str:
        ...

    @abstractmethod
    def decrypt_data(self, *, from_key: PublicKey, to_key: PublicKey, content: str) -> str:
        ...

    def __enter__(self) -> Self:
        return self

    def __exit__(
        self,
        _: type[BaseException] | None,
        exception: BaseException | None,
        traceback: TracebackType | None,
    ) -> None:
        self.lock()
