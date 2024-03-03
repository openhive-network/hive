from __future__ import annotations

import warnings
from dataclasses import dataclass, field
from typing import TYPE_CHECKING, Any, Protocol

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
    def __init__(self, *args: Any, **kwargs: Any) -> None:
        super().__init__(*args, **kwargs)

    def _http_webserver_ready(self, notification: Notification[WebserverListening]) -> None:
        self.__empty_handle_message(notification.value)

    def _handle_error(self, error: Error) -> None:
        self.__empty_handle_message(error)

    def _handle_status_change(self, status: Status) -> None:
        self.__empty_handle_message(status)

    def _handle_wallets_closed(self, note: AttemptClosingWallets) -> None:
        self.__empty_handle_message(note)

    def _handle_opening_beekeeper_failed(self, info: OpeningBeekeeperFailed) -> None:
        self.__empty_handle_message(info)

    def __empty_handle_message(self, obj: Any) -> None:
        warnings.warn(f"Notification `{type(obj).__name__}` hasn't been handled", category=RuntimeWarning, stacklevel=1)
