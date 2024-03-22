from __future__ import annotations

from typing import TYPE_CHECKING, cast

from loguru import logger

import helpy
from beekeepy._handle.beekeeper import AsyncBeekeeper as AsynchronousBeekeeperHandle
from beekeepy._handle.beekeeper import Beekeeper as SynchronousBeekeeperHandle
from beekeepy._handle.beekeeper import BeekeeperAlreadyRunningError
from beekeepy._interface.abc.packed_object import Packed, _RemoteFactoryCallable
from beekeepy._interface.asynchronous.beekeeper import (
    Beekeeper as AsynchronousBeekeeper,
)
from beekeepy._interface.synchronous.beekeeper import Beekeeper as SynchronousBeekeeper
from beekeepy.settings import Settings
from helpy._interfaces.url import Url

if TYPE_CHECKING:
    from loguru import Logger

    from beekeepy._interface.abc.asynchronous.beekeeper import (
        Beekeeper as AsynchronousBeekeeperInterface,
    )
    from beekeepy._interface.abc.synchronous.beekeeper import (
        Beekeeper as SynchronousBeekeeperInterface,
    )
    from helpy import HttpUrl


def _get_logger() -> Logger:
    return logger


PackedBeekeeper = Packed[SynchronousBeekeeper]
PackedAsyncBeekeeper = Packed[AsynchronousBeekeeper]


class _SynchronousBeekeeperImpl(SynchronousBeekeeper):
    def pack(self) -> PackedBeekeeper:  # type: ignore
        return Packed(settings=cast(Settings, self._get_instance().settings), unpack_factory=beekeeper_remote_factory)  # type: ignore


class _AsynchronousBeekeeperImpl(AsynchronousBeekeeper):
    def pack(self) -> PackedAsyncBeekeeper:  # type: ignore
        return Packed(settings=cast(Settings, self._get_instance().settings), unpack_factory=async_beekeeper_remote_factory)  # type: ignore


def _beekeeper_factory_impl(
    impl_t: type[_SynchronousBeekeeperImpl] | type[_AsynchronousBeekeeperImpl],
    handle_t: type[SynchronousBeekeeperHandle] | type[AsynchronousBeekeeperHandle],
    remote_factory: _RemoteFactoryCallable,
    settings: Settings | None = None,
) -> SynchronousBeekeeperInterface | AsynchronousBeekeeperInterface:
    settings = settings or Settings()
    beekeeper = handle_t(settings=settings, logger=_get_logger())
    try:
        beekeeper.run()
    except BeekeeperAlreadyRunningError as err:
        settings.http_endpoint = err.address
        return remote_factory(url_or_settings=settings)
    return impl_t(handle=beekeeper)  # type: ignore


def _beekeeper_remote_factory_impl(
    impl_t: type[_SynchronousBeekeeperImpl] | type[_AsynchronousBeekeeperImpl],
    handle_t: type[helpy.Beekeeper] | type[helpy.AsyncBeekeeper],
    url_or_settings: HttpUrl | Settings | None = None,
) -> SynchronousBeekeeperInterface | AsynchronousBeekeeperInterface:
    if isinstance(url_or_settings, Url):
        url_or_settings = Settings(http_endpoint=url_or_settings)
    return impl_t(handle=handle_t(settings=url_or_settings))  # type: ignore


def beekeeper_factory(*, settings: Settings | None = None) -> SynchronousBeekeeperInterface:
    """Creates an beekeeper object, which allows to manage keys and sign with them.

    Note:
        Remember to call .delete() if you don't use with statement

    Keyword Arguments:
        settings -- Adjustable parameters of connection and workspace (default: {None})
    """
    return _beekeeper_factory_impl(_SynchronousBeekeeperImpl, SynchronousBeekeeperHandle, beekeeper_remote_factory, settings)  # type: ignore[return-value]


def beekeeper_remote_factory(*, url_or_settings: HttpUrl | Settings) -> SynchronousBeekeeperInterface:
    """Same as beekeeper_factory, but allows quickly to connect to remote beekeeper instance."""
    return _beekeeper_remote_factory_impl(_SynchronousBeekeeperImpl, helpy.Beekeeper, url_or_settings)  # type: ignore[return-value]


def async_beekeeper_factory(*, settings: Settings | None = None) -> AsynchronousBeekeeperInterface:
    """Same as beekeeper_factory but in async version."""
    return _beekeeper_factory_impl(_AsynchronousBeekeeperImpl, AsynchronousBeekeeperHandle, async_beekeeper_remote_factory, settings)  # type: ignore[return-value]


def async_beekeeper_remote_factory(*, url_or_settings: HttpUrl | Settings) -> AsynchronousBeekeeperInterface:
    """Same as beekeeper_remote_factory but in async version."""
    return _beekeeper_remote_factory_impl(_AsynchronousBeekeeperImpl, helpy.AsyncBeekeeper, url_or_settings)  # type: ignore[return-value]
