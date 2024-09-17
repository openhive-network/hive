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
    @abstractmethod
    def _factory(cls, *, settings: Settings | None = None) -> Beekeeper: ...

    @classmethod
    def factory(cls, *, settings: Settings | None = None) -> Beekeeper:
        from beekeepy._interface.synchronous.beekeeper import Beekeeper as BeekeeperImplementation
        return BeekeeperImplementation._factory(settings=settings)  # noqa: SLF001

    @classmethod
    @abstractmethod
    def _remote_factory(cls, *, url_or_settings: Settings | HttpUrl) -> Beekeeper: ...

    @classmethod
    def remote_factory(cls, *, url_or_settings: Settings | HttpUrl) -> Beekeeper:
        from beekeepy._interface.synchronous.beekeeper import Beekeeper as BeekeeperImplementation
        return BeekeeperImplementation._remote_factory(url_or_settings=url_or_settings)  # noqa: SLF001
