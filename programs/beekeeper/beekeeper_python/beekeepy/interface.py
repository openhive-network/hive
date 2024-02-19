# type: ignore
# ruff: noqa

from __future__ import annotations

from abc import ABC, abstractmethod
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from types import TracebackType

    from typing_extensions import Self

    from schemas.fields.basic import PublicKey


class Wallet(ABC):
    @abstractmethod
    def generate_key(self, *, salt: str | None = None) -> PublicKey: ...

    @abstractmethod
    def import_key(self, *, private_key: str) -> PublicKey: ...

    @abstractmethod
    def sign_digest(self, *, digest: str, key: PublicKey) -> str: ...

    @property
    @abstractmethod
    def public_keys(self) -> list[PublicKey]: ...

    @property
    @abstractmethod
    def is_unlocked(self) -> bool: ...

    @abstractmethod
    def __enter__(self) -> Self:
        """Does nothing."""

    @abstractmethod
    def __exit__(
        self,
        _: type[BaseException] | None,
        exception: BaseException | None,
        traceback: TracebackType | None,
    ) -> bool | None:
        """Closes wallet."""


class Session(ABC):
    @abstractmethod
    def create_wallet(self, *, name: str, password: str) -> Wallet: ...

    @abstractmethod
    def open_wallet(self, *, name: str, password: str) -> Wallet: ...

    @abstractmethod
    def close_session() -> None: ...

    @abstractmethod
    def __enter__(self) -> Self:
        """Does nothing."""

    @abstractmethod
    def __exit__(
        self,
        _: type[BaseException] | None,
        exception: BaseException | None,
        traceback: TracebackType | None,
    ) -> bool | None:
        """Closes session."""


class Beekeeper(ABC):
    @abstractmethod
    def create_session(self, *, salt: str | None = None) -> Session: ...
