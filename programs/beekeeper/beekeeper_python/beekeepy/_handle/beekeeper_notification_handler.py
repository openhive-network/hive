from __future__ import annotations

from typing import TYPE_CHECKING, Any

import helpy
from schemas.notifications import (
    AttemptClosingWallets,
    Error,
    KnownNotificationT,
    Notification,
    OpeningBeekeeperFailed,
    Status,
    WebserverListening,
)

if TYPE_CHECKING:
    from beekeepy._handle.beekeeper_callbacks import BeekeeperCallbacks

import test_tools as tt


class NotificationHandler(helpy.BeekeeperNotificationHandler):
    def __init__(self, owner: BeekeeperCallbacks, *args: Any, **kwargs: Any) -> None:
        super().__init__(*args, **kwargs)
        self.__owner = owner

    async def on_attempt_of_closing_wallets(self, notification: Notification[AttemptClosingWallets]) -> None:
        self.__owner._call(AttemptClosingWallets, notification)

    async def on_opening_beekeeper_failed(self, notification: Notification[OpeningBeekeeperFailed]) -> None:
        self.__owner._call(OpeningBeekeeperFailed, notification)

    async def on_error(self, notification: Notification[Error]) -> None:
        self.__owner._handle_error(notification.value)

    async def on_status_changed(self, notification: Notification[Status]) -> None:
        self.__owner._handle_status_change(notification.value)

    async def on_http_webserver_bind(self, notification: Notification[WebserverListening]) -> None:
        self.__owner._http_webserver_ready(notification)

    async def handle_notification(self, notification: Notification[KnownNotificationT]) -> None:
        tt.logger.info(f"got notification: {notification.json()}")
        return await super().handle_notification(notification)
