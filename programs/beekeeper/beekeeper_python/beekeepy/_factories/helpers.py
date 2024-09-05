from __future__ import annotations

from typing import TYPE_CHECKING, cast

from loguru import logger

from beekeepy._interface.settings import Settings
from helpy._interfaces.url import Url

if TYPE_CHECKING:
    from loguru import Logger

    from beekeepy._handle.beekeeper import AsyncRemoteBeekeeper as AsynchronousRemoteBeekeeperHandle
    from beekeepy._handle.beekeeper import SyncRemoteBeekeeper as SynchronousRemoteBeekeeperHandle
    from helpy import Settings as HelpySettings


def _prepare_settings_for_packing(settings: HelpySettings) -> Settings:
    settings = cast(Settings, settings)
    if not isinstance(settings, Url):
        settings.notification_endpoint = None
    return settings


def _get_logger() -> Logger:
    return logger


def _use_existing_session_token(
    settings: Settings, handle: SynchronousRemoteBeekeeperHandle | AsynchronousRemoteBeekeeperHandle
) -> None:
    if settings.use_existing_session is not None:
        handle.set_session_token(settings.use_existing_session)
