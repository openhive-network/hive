from __future__ import annotations

from threading import Event
from typing import TYPE_CHECKING, Any

from loguru import logger

import helpy

if TYPE_CHECKING:
    from beekeepy._handle.beekeeper_callbacks import BeekeeperNotificationCallbacks
    from schemas.notifications import (
        AttemptClosingWallets,
        Error,
        KnownNotificationT,
        Notification,
        OpeningBeekeeperFailed,
        Status,
        WebserverListening,
    )


class NotificationHandler(helpy.BeekeeperNotificationHandler):
    def __init__(self, owner: BeekeeperNotificationCallbacks, *args: Any, **kwargs: Any) -> None:
        super().__init__(*args, **kwargs)
        self.__owner = owner

        self.http_listening_event = Event()
        self.http_endpoint_from_event: helpy.HttpUrl | None = None

        self.already_working_beekeeper_event = Event()
        self.already_working_beekeeper_http_address: helpy.HttpUrl | None = None
        self.already_working_beekeeper_pid: int | None = None

    async def on_attempt_of_closing_wallets(self, notification: Notification[AttemptClosingWallets]) -> None:
        self.__owner._handle_wallets_closed(notification.value)

    async def on_opening_beekeeper_failed(self, notification: Notification[OpeningBeekeeperFailed]) -> None:
        self.already_working_beekeeper_http_address = helpy.HttpUrl(
            self.__combine_url_string(
                notification.value.connection.address,
                notification.value.connection.port,
            ),
            protocol="http",
        )
        self.already_working_beekeeper_pid = int(notification.value.pid)
        self.already_working_beekeeper_event.set()
        self.__owner._handle_opening_beekeeper_failed(notification.value)

    async def on_error(self, notification: Notification[Error]) -> None:
        self.__owner._handle_error(notification.value)

    async def on_status_changed(self, notification: Notification[Status]) -> None:
        self.__owner._handle_status_change(notification.value)

    async def on_http_webserver_bind(self, notification: Notification[WebserverListening]) -> None:
        self.http_endpoint_from_event = helpy.HttpUrl(
            self.__combine_url_string(notification.value.address, notification.value.port),
            protocol="http",
        )
        self.http_listening_event.set()
        self.__owner._http_webserver_ready(notification)

    async def handle_notification(self, notification: Notification[KnownNotificationT]) -> None:
        logger.debug(f"got notification: {notification.json()}")
        return await super().handle_notification(notification)

    def __combine_url_string(self, address: str, port: int) -> str:
        return f"{address}:{port}"
