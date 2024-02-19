from __future__ import annotations

from abc import abstractmethod
from dataclasses import dataclass, field
from threading import Event
from typing import Any, Protocol

import helpy
from schemas.notifications import (
    AttemptClosingWallets,
    Error,
    Notification,
    OpeningBeekeeperFailed,
    Status,
    WebserverListening,
)


class NotificationCallback(Protocol):
    def __call__(self, notification: Notification[Any]) -> None: ...


@dataclass
class CallbackCollection:
    _id_counter: int = 0
    callbacks: dict[int, NotificationCallback] = field(default_factory=dict)

    def add(self, callback: NotificationCallback) -> int:
        self._id_counter += 1
        self.callbacks[self._id_counter] = callback
        return self._id_counter

    def remove(self, callback_id: int) -> None:
        self.callbacks.pop(callback_id)

    def call(self, notification: Notification[Any]) -> None:
        for callback in self.callbacks.values():
            callback(notification)


class BeekeeperCallbacks:
    SUPPORTED_NOTIFICATIONS = type[AttemptClosingWallets] | type[OpeningBeekeeperFailed] | type[Error]

    def __init__(self, *args: Any, **kwargs: Any) -> None:
        self.__callbacks: dict[Any, CallbackCollection] = {}
        self._http_listening_event = Event()
        self._http_endpoint: helpy.HttpUrl | None = None
        super().__init__(*args, **kwargs)

    def register_callback(self, on: SUPPORTED_NOTIFICATIONS, callback: NotificationCallback) -> int:
        return self.__callbacks[on].add(callback)

    def unregister_callback(self, on: SUPPORTED_NOTIFICATIONS, callback_id: int) -> None:
        if on in self.__callbacks:
            self.__callbacks[on].remove(callback_id)

    def _call(
        self,
        notification_type: SUPPORTED_NOTIFICATIONS,
        notification: Notification[Any],
    ) -> None:
        if notification_type in self.__callbacks:
            self.__callbacks[notification_type].call(notification)

    def _http_webserver_ready(self, notification: Notification[WebserverListening]) -> None:
        self._http_endpoint = helpy.HttpUrl(self.__combine_url_string_from_notification(notification), protocol="http")
        self._http_listening_event.set()

    def __combine_url_string_from_notification(self, notification: Notification[WebserverListening]) -> str:
        return f"{notification.value.address}:{notification.value.port}"

    @abstractmethod
    def _handle_error(self, error: Error) -> None: ...

    @abstractmethod
    def _handle_status_change(self, status: Status) -> None: ...
