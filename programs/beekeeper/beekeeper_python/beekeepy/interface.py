# type: ignore
# ruff: noqa

from __future__ import annotations

from abc import ABC, abstractmethod
from typing import TYPE_CHECKING

import helpy

if TYPE_CHECKING:
    from types import TracebackType

    from typing_extensions import Self

    from helpy import HttpUrl
    from schemas.fields.basic import PublicKey
    from schemas.apis.beekeeper_api import GetInfo


class Wallet(ABC):
    @property
    @abstractmethod
    def public_keys(self) -> list[PublicKey]:
        ...

    @property
    def unlocked(self) -> UnlockedWallet | None:
        ...

    @abstractmethod
    def unlock(self, password: str) -> UnlockedWallet:
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

    def __enter__(self) -> UnlockedWallet:
        return self

    def __exit__(
        self,
        _: type[BaseException] | None,
        exception: BaseException | None,
        traceback: TracebackType | None,
    ) -> None:
        self.lock()


class Session(ABC):
    @abstractmethod
    def get_info(self) -> GetInfo:
        ...

    @abstractmethod
    def create_wallet(self, *, name: str, password: str) -> UnlockedWallet:
        ...

    @abstractmethod
    def open_wallet(self, *, name: str, password: str) -> UnlockedWallet:
        ...

    @abstractmethod
    def close_session(self) -> None:
        ...

    @abstractmethod
    def lock_all(self) -> list[Wallet]:
        ...

    @property
    @abstractmethod
    def wallets(self) -> list[Wallet]:
        ...

    @abstractmethod
    def __enter__(self) -> Self:
        """Does nothing."""

    @abstractmethod
    def __exit__(
        self,
        _: type[BaseException] | None,
        exception: BaseException | None,
        traceback: TracebackType | None,
    ) -> bool:
        """Closes session."""


class Beekeeper(ABC):
    @abstractmethod
    def _get_implementation(self) -> helpy.Beekeeper:
        """Returns implementation of beekeeper"""

    @abstractmethod
    def create_session(self, *, salt: str | None = None) -> Session:
        ...


class RemoteBeekeeper(Beekeeper, ABC):
    def __init__(self, *, url: HttpUrl) -> None:
        """Just sets url and validates is working"""


class ServiceBeekeeper(Beekeeper, ABC):
    def __init__(self) -> None:
        """Creates Beekeeper Service"""


def beekeeper_factory() -> Beekeeper:
    """Contains logic to setup beekeeper as full service or switches to remote"""
    return ServiceBeekeeper()


def beekeeper_remote_factory(*, url: HttpUrl) -> Beekeeper:
    return RemoteBeekeeper(url=url)
