from __future__ import annotations

from typing import TYPE_CHECKING, Generic, Protocol, TypeVar

from beekeepy._interface.abc.asynchronous.beekeeper import (
    Beekeeper as AsynchronousBeekeeperInterface,
)
from beekeepy._interface.abc.synchronous.beekeeper import (
    Beekeeper as SynchronousBeekeeperInterface,
)

if TYPE_CHECKING:
    from beekeepy.settings import Settings
    from helpy import HttpUrl

T_co = TypeVar("T_co", bound=SynchronousBeekeeperInterface | AsynchronousBeekeeperInterface, covariant=True)


class _RemoteFactoryCallable(Protocol[T_co]):
    def __call__(self, *, url_or_settings: HttpUrl | Settings) -> T_co: ...


class Packed(Generic[T_co]):
    """Allows to transfer beekeeper handle to other process."""

    def __init__(self, settings: Settings, unpack_factory: _RemoteFactoryCallable[T_co]) -> None:
        self.__settings = settings
        self.__unpack_factory = unpack_factory

    def unpack(self) -> T_co:
        return self.__unpack_factory(url_or_settings=self.__settings)
