from __future__ import annotations

from typing import TYPE_CHECKING, Any

import helpy
from beekeepy._executable import BeekeeperExecutable
from beekeepy._handle.beekeeper_callbacks import BeekeeperCallbacks
from beekeepy._handle.beekeeper_notification_handler import NotificationHandler
from helpy._communication.universal_notification_server import (
    UniversalNotificationServer,
)
from helpy.exceptions import HelpyError

if TYPE_CHECKING:
    from pathlib import Path

    from loguru import Logger

    from schemas.notifications import (
        Error,
        OpeningBeekeeperFailed,
        Status,
    )


class BeekeeperAlreadyRunningError(HelpyError):
    def __init__(self, *args: object, address: helpy.HttpUrl) -> None:
        super().__init__(*args)
        self.address = address


class BeekeeperCommon(BeekeeperCallbacks):
    def __init__(self, *args: Any, working_directory: Path, logger: Logger, **kwargs: Any) -> None:
        super().__init__(*args, **kwargs)
        self.__exec = BeekeeperExecutable(working_directory, logger)
        self.__notification: UniversalNotificationServer | None = None
        self.__notification_event_handler: NotificationHandler | None = None

    def __create_notification_server(self) -> UniversalNotificationServer:
        assert self.__notification is None
        assert self.__notification_event_handler is None

        self.__notification_event_handler = NotificationHandler(self)
        return UniversalNotificationServer(self.__notification_event_handler)

    def __destroy_notification_server(self) -> None:
        if self.__notification is not None:
            self.__notification.close()
            self.__notification = None

        if self.__notification_event_handler is not None:
            self.__notification_event_handler = None

    def __wait_till_ready(self) -> None:
        assert self.__notification_event_handler is not None
        try:
            self.__notification_event_handler.http_listening_event.wait(timeout=5)
        except TimeoutError as err:
            if self.__notification_event_handler.already_working_beekeeper_event.is_set():
                assert (addr := self.__notification_event_handler.already_working_beekeeper_http_address) is not None
                raise BeekeeperAlreadyRunningError(address=addr) from err

    def _handle_error(self, error: Error) -> None:
        pass

    def _handle_status_change(self, status: Status) -> None:
        pass

    def _handle_opening_beekeeper_failed(self, info: OpeningBeekeeperFailed) -> None:
        return super()._handle_opening_beekeeper_failed(info)

    def run(self) -> None:
        self.__notification = self.__create_notification_server()
        self.__notification.run()
        self.__exec.run(
            blocking=False,
            arguments=[
                "--notifications-endpoint",
                self.notification_endpoint,
                "--webserver-http-endpoint",
                "127.0.0.1:0",
                "-d",
                self.__exec.woring_dir.as_posix(),
            ],
        )
        try:
            self.__wait_till_ready()
        except BeekeeperAlreadyRunningError:
            self.close()
            raise

    def close(self) -> None:
        assert self.__notification is not None
        self.__exec.close()
        self.__destroy_notification_server()

    def _get_http_endpoint_from_event(self) -> helpy.HttpUrl:
        assert self.__notification_event_handler is not None
        assert (addr := self.__notification_event_handler.http_endpoint_from_event) is not None
        return addr

    @property
    def notification_endpoint(self) -> str:
        assert self.__notification is not None
        return f"127.0.0.1:{self.__notification.port}"


class Beekeeper(BeekeeperCommon, helpy.Beekeeper):
    def __init__(self, *args: Any, working_directory: Path, logger: Logger, **kwargs: Any) -> None:
        super().__init__(*args, working_directory=working_directory, logger=logger, **kwargs)

    def run(self) -> None:
        super().run()
        self.http_endpoint = self._get_http_endpoint_from_event()

    def _get_notification_endpoint(self) -> str:
        return self.notification_endpoint


class AsyncBeekeeper(BeekeeperCommon, helpy.AsyncBeekeeper):
    def __init__(self, *args: Any, working_directory: Path, logger: Logger, **kwargs: Any) -> None:
        super().__init__(*args, working_directory=working_directory, logger=logger, **kwargs)

    def run(self) -> None:
        super().run()
        self.http_endpoint = self._get_http_endpoint_from_event()

    def _get_notification_endpoint(self) -> str:
        return self.notification_endpoint
