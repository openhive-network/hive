from __future__ import annotations

from abc import ABC, abstractmethod
from typing import TYPE_CHECKING

from helpy import ContextSync, HttpUrl

if TYPE_CHECKING:
    from beekeepy._interface.abc.packed_object import PackedSyncBeekeeper
    from beekeepy._interface.abc.synchronous.session import Session
    from beekeepy._interface.settings import Settings


class Beekeeper(ContextSync["Beekeeper"], ABC):
    @abstractmethod
    def create_session(self, *, salt: str | None = None) -> Session: ...

    @property
    @abstractmethod
    def session(self) -> Session: ...

    def _enter(self) -> Beekeeper:
        return self

    def _finally(self) -> None:
        self.teardown()

    @abstractmethod
    def teardown(self) -> None: ...

    @abstractmethod
    def pack(self) -> PackedSyncBeekeeper: ...

    @abstractmethod
    def detach(self) -> None: ...

    @classmethod
    def factory(cls, *, settings: Settings | None = None) -> Beekeeper:
        raise NotImplementedError

    @classmethod
    def remote_factory(cls, *, url_or_settings: Settings | HttpUrl) -> Beekeeper:
        raise NotImplementedError
