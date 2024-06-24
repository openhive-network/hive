from __future__ import annotations

import errno
import math
import os
import signal
import time
from typing import TYPE_CHECKING, cast

from loguru import logger

from beekeepy._handle.beekeeper import AsyncBeekeeper as AsynchronousBeekeeperHandle
from beekeepy._handle.beekeeper import AsyncRemoteBeekeeper as AsynchronousRemoteBeekeeperHandle
from beekeepy._handle.beekeeper import Beekeeper as SynchronousBeekeeperHandle
from beekeepy.exceptions import BeekeeperAlreadyRunningError
from beekeepy._handle.beekeeper import SyncRemoteBeekeeper as SynchronousRemoteBeekeeperHandle
from beekeepy._interface.abc.asynchronous.beekeeper import (
    Beekeeper as AsynchronousBeekeeperInterface,
)
from beekeepy._interface.abc.packed_object import Packed, _RemoteFactoryCallable
from beekeepy._interface.abc.synchronous.beekeeper import (
    Beekeeper as SynchronousBeekeeperInterface,
)
from beekeepy._interface.asynchronous.beekeeper import (
    Beekeeper as AsynchronousBeekeeper,
)
from beekeepy._interface.synchronous.beekeeper import Beekeeper as SynchronousBeekeeper
from beekeepy.settings import Settings
from helpy._interfaces.url import Url

if TYPE_CHECKING:
    from pathlib import Path

    from loguru import Logger

    from helpy import HttpUrl
    from helpy import Settings as HelpySettings


def _get_logger() -> Logger:
    return logger


PackedBeekeeper = Packed[SynchronousBeekeeperInterface]
PackedAsyncBeekeeper = Packed[AsynchronousBeekeeperInterface]


def _update_settings(settings: HelpySettings) -> Settings:
    settings = cast(Settings, settings)
    if not isinstance(settings, Url):
        settings.notification_endpoint = None
    return settings


class _SynchronousBeekeeperImpl(SynchronousBeekeeper):
    def pack(self) -> PackedBeekeeper:
        return Packed(settings=_update_settings(self._get_instance().settings), unpack_factory=beekeeper_remote_factory)


class _AsynchronousBeekeeperImpl(AsynchronousBeekeeper):
    def pack(self) -> PackedAsyncBeekeeper:
        return Packed(
            settings=_update_settings(self._get_instance().settings), unpack_factory=async_beekeeper_remote_factory
        )


def _beekeeper_factory_impl(
    impl_t: type[_SynchronousBeekeeperImpl | _AsynchronousBeekeeperImpl],
    handle_t: type[SynchronousBeekeeperHandle | AsynchronousBeekeeperHandle],
    remote_factory: _RemoteFactoryCallable,
    settings: Settings | None = None,
) -> SynchronousBeekeeperInterface | AsynchronousBeekeeperInterface:
    settings = settings or Settings()
    beekeeper = handle_t(settings=settings, logger=_get_logger())
    try:
        beekeeper.run()
    except BeekeeperAlreadyRunningError as err:
        settings.http_endpoint = err.address
        return remote_factory(url_or_settings=settings)  # type: ignore[no-any-return]
    return impl_t(handle=beekeeper)  # type: ignore[arg-type]


def _beekeeper_remote_factory_impl(
    impl_t: type[_SynchronousBeekeeperImpl | _AsynchronousBeekeeperImpl],
    handle_t: type[SynchronousRemoteBeekeeperHandle | AsynchronousRemoteBeekeeperHandle],
    url_or_settings: HttpUrl | Settings | None = None,
) -> SynchronousBeekeeperInterface | AsynchronousBeekeeperInterface:
    if isinstance(url_or_settings, Url):
        url_or_settings = Settings(http_endpoint=url_or_settings)
    handle = handle_t(settings=url_or_settings, logger=_get_logger())  # type: ignore[arg-type]
    handle.run()
    return impl_t(handle=handle)  # type: ignore


def beekeeper_factory(*, settings: Settings | None = None) -> SynchronousBeekeeperInterface:
    """Creates an beekeeper object, which allows to manage keys and sign with them.

    Note:
        Remember to call .delete() if you don't use with statement

    Keyword Arguments:
        settings: Adjustable parameters of connection and workspace (default: {None})
    """
    return _beekeeper_factory_impl(_SynchronousBeekeeperImpl, SynchronousBeekeeperHandle, beekeeper_remote_factory, settings)  # type: ignore[return-value]


def beekeeper_remote_factory(*, url_or_settings: HttpUrl | Settings) -> SynchronousBeekeeperInterface:
    """Same as beekeeper_factory, but allows quickly to connect to remote beekeeper instance."""
    return _beekeeper_remote_factory_impl(_SynchronousBeekeeperImpl, SynchronousRemoteBeekeeperHandle, url_or_settings)  # type: ignore[return-value]


def async_beekeeper_factory(*, settings: Settings | None = None) -> AsynchronousBeekeeperInterface:
    """Same as beekeeper_factory but in async version."""
    return _beekeeper_factory_impl(_AsynchronousBeekeeperImpl, AsynchronousBeekeeperHandle, async_beekeeper_remote_factory, settings)  # type: ignore[return-value]


def async_beekeeper_remote_factory(*, url_or_settings: HttpUrl | Settings) -> AsynchronousBeekeeperInterface:
    """Same as beekeeper_remote_factory but in async version."""
    return _beekeeper_remote_factory_impl(_AsynchronousBeekeeperImpl, AsynchronousRemoteBeekeeperHandle, url_or_settings)  # type: ignore[return-value]


def close_already_running_beekeeper(*, working_directory: Path) -> None:
    """If beekeeper has been started and explicitly not closed, this function allows to close it basing on workdir."""

    def wait_for_pid_to_die(pid: int, *, timeout_secs: float = math.inf) -> None:
        sleep_time = min(1.0, timeout_secs)
        already_waited = 0.0
        while not is_running(pid):
            if timeout_secs - already_waited <= 0:
                raise TimeoutError(f"Process with pid {pid} didn't die in {timeout_secs} seconds.")

            time.sleep(sleep_time)
            already_waited += sleep_time

    try:
        SynchronousBeekeeperHandle(settings=Settings(working_directory=working_directory), logger=_get_logger())
    except BeekeeperAlreadyRunningError as err:
        sig = signal.SIGINT
        pid = err.pid
        os.kill(pid, sig)

        try:
            wait_for_pid_to_die(pid, timeout_secs=10)
            logger.debug("Process was closed with SIGINT")
        except TimeoutError:
            sig = signal.SIGKILL
            os.kill(pid, sig)
            wait_for_pid_to_die(pid)
            logger.debug("Process was force-closed with SIGKILL")

    def is_running(pid: int) -> bool:
        """
        Check whether pid exists in the current process table.

        Source: https://stackoverflow.com/a/7654102

        Args:
        ----
        pid: The Process ID to check.

        Returns:
        -------
        True if process with the given pid is running else False.
        """
        try:
            os.kill(pid, 0)
        except OSError as err:
            if err.errno == errno.ESRCH:
                return False
        return True
