from __future__ import annotations

import tempfile
from pathlib import Path
from typing import TYPE_CHECKING

from loguru import logger

import helpy
from beekeepy._handle.beekeeper import AsyncBeekeeper as AsynchronousBeekeeperHandle
from beekeepy._handle.beekeeper import Beekeeper as SynchronousBeekeeperHandle
from beekeepy._handle.beekeeper import BeekeeperAlreadyRunningError
from beekeepy._interface.asynchronous.beekeeper import Beekeeper as AsynchronousBeekeeper
from beekeepy._interface.synchronous.beekeeper import Beekeeper as SynchronousBeekeeper

if TYPE_CHECKING:
    from loguru import Logger

    from beekeepy._interface.abc.asynchronous.beekeeper import Beekeeper as AsynchronousBeekeeperInterface
    from beekeepy._interface.abc.synchronous.beekeeper import Beekeeper as SynchronousBeekeeperInterface
    from helpy import HttpUrl


def _get_logger() -> Logger:
    return logger


def _get_working_directory() -> Path:
    return Path(tempfile.mkdtemp(dir=Path(".").absolute()))


def beekeeper_factory() -> SynchronousBeekeeperInterface:
    beekeeper = SynchronousBeekeeperHandle(working_directory=_get_working_directory(), logger=_get_logger())
    try:
        beekeeper.run()
    except BeekeeperAlreadyRunningError as err:
        return beekeeper_remote_factory(url=err.address)
    return SynchronousBeekeeper(handle=beekeeper)


def beekeeper_remote_factory(*, url: HttpUrl) -> SynchronousBeekeeperInterface:
    return SynchronousBeekeeper(handle=helpy.Beekeeper(http_url=url))


def async_beekeeper_factory() -> AsynchronousBeekeeperInterface:
    beekeeper = AsynchronousBeekeeperHandle(working_directory=_get_working_directory(), logger=_get_logger())
    try:
        beekeeper.run()
    except BeekeeperAlreadyRunningError as err:
        return async_beekeeper_remote_factory(url=err.address)
    return AsynchronousBeekeeper(handle=beekeeper)


def async_beekeeper_remote_factory(*, url: HttpUrl) -> AsynchronousBeekeeperInterface:
    return AsynchronousBeekeeper(handle=helpy.AsyncBeekeeper(http_url=url))
