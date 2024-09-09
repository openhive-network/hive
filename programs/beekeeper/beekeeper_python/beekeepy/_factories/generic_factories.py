from __future__ import annotations

from typing import TYPE_CHECKING

from beekeepy._factories.helpers import _get_logger, _use_existing_session_token
from beekeepy._handle.beekeeper import AsyncBeekeeper as AsynchronousBeekeeperHandle
from beekeepy._handle.beekeeper import AsyncRemoteBeekeeper as AsynchronousRemoteBeekeeperHandle
from beekeepy._handle.beekeeper import Beekeeper as SynchronousBeekeeperHandle
from beekeepy._handle.beekeeper import SyncRemoteBeekeeper as SynchronousRemoteBeekeeperHandle
from beekeepy._handle.beekeeper import run_if_possible
from beekeepy._interface.settings import Settings
from beekeepy.exceptions import BeekeeperAlreadyRunningError
from helpy._interfaces.url import Url

if TYPE_CHECKING:
    from beekeepy._factories.beekeeper_factories import _AsynchronousBeekeeperImpl, _SynchronousBeekeeperImpl
    from beekeepy._interface.abc.asynchronous.beekeeper import (
        Beekeeper as AsynchronousBeekeeperInterface,
    )
    from beekeepy._interface.abc.packed_object import _RemoteFactoryCallable
    from beekeepy._interface.abc.synchronous.beekeeper import (
        Beekeeper as SynchronousBeekeeperInterface,
    )
    from helpy import HttpUrl


def _beekeeper_factory_impl(
    impl_t: type[_SynchronousBeekeeperImpl | _AsynchronousBeekeeperImpl],
    handle_t: type[SynchronousBeekeeperHandle | AsynchronousBeekeeperHandle],
    remote_factory: (
        _RemoteFactoryCallable[SynchronousBeekeeperInterface] | _RemoteFactoryCallable[AsynchronousBeekeeperInterface]
    ),
    settings: Settings | None = None,
) -> SynchronousBeekeeperInterface | AsynchronousBeekeeperInterface:
    settings = settings or Settings()
    beekeeper = handle_t(settings=settings, logger=_get_logger())
    _use_existing_session_token(settings, beekeeper)
    try:
        run_if_possible(beekeeper)
    except BeekeeperAlreadyRunningError as err:
        settings.http_endpoint = err.address
        return remote_factory(url_or_settings=settings)
    return impl_t(handle=beekeeper)  # type: ignore[arg-type]


def _beekeeper_remote_factory_impl(
    impl_t: type[_SynchronousBeekeeperImpl | _AsynchronousBeekeeperImpl],
    handle_t: type[SynchronousRemoteBeekeeperHandle | AsynchronousRemoteBeekeeperHandle],
    url_or_settings: HttpUrl | Settings,
) -> SynchronousBeekeeperInterface | AsynchronousBeekeeperInterface:
    settings = Settings(http_endpoint=url_or_settings) if isinstance(url_or_settings, Url) else url_or_settings
    handle = handle_t(settings=url_or_settings)  # type: ignore[arg-type]
    _use_existing_session_token(settings, handle)
    run_if_possible(handle)
    return impl_t(handle=handle)  # type: ignore
