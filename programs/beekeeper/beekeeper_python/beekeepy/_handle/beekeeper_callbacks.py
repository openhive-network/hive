from __future__ import annotations

import warnings
from abc import ABC
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


class BeekeeperNotificationCallbacks(ABC):  # noqa: B024
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
