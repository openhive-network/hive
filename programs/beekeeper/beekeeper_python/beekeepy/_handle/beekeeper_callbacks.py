from __future__ import annotations

from abc import abstractmethod
from dataclasses import dataclass, field
from threading import Event
from typing import TYPE_CHECKING, Any, Protocol

import helpy

if TYPE_CHECKING:
    from schemas.notifications import (
        AttemptClosingWallets,
        Error,
        Notification,
        OpeningBeekeeperFailed,
        Status,
        WebserverListening,
    )

class NotificationCallback(Protocol):
    def __call__(self, notification: Notification[Any]) -> None:
        ...


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
    def __init__(self, *args: Any, **kwargs: Any) -> None:
        self._http_endpoint_from_event: helpy.HttpUrl | None = None
        self._already_working_beekeeper_http_address: helpy.HttpUrl | None = None
        super().__init__(*args, **kwargs)

    @abstractmethod
    def _http_webserver_ready(self, notification: Notification[WebserverListening]) -> None:
        ...

    @abstractmethod
    def _handle_error(self, error: Error) -> None:
        ...

    @abstractmethod
    def _handle_status_change(self, status: Status) -> None:
        ...

    @abstractmethod
    def _handle_wallets_closed(self, note: AttemptClosingWallets) -> None:
        ...

    @abstractmethod
    def _handle_opening_beekeeper_failed(self, info: OpeningBeekeeperFailed) -> None:
        ...
