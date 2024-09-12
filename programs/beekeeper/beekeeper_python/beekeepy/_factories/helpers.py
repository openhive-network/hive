from __future__ import annotations

from typing import TYPE_CHECKING

from loguru import logger

if TYPE_CHECKING:
    from loguru import Logger

    from beekeepy._handle.beekeeper import AsyncRemoteBeekeeper as AsynchronousRemoteBeekeeperHandle
    from beekeepy._handle.beekeeper import SyncRemoteBeekeeper as SynchronousRemoteBeekeeperHandle
    from beekeepy._interface.settings import Settings


def _get_logger() -> Logger:
    return logger


def _use_existing_session_token(
    settings: Settings, handle: SynchronousRemoteBeekeeperHandle | AsynchronousRemoteBeekeeperHandle
) -> None:
    if settings.use_existing_session is not None:
        handle.set_session_token(settings.use_existing_session)
