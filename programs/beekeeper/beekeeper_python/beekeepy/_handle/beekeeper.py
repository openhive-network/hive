from __future__ import annotations

from typing import TYPE_CHECKING, Any

import helpy
from beekeepy._executable import BeekeeperExecutable
from beekeepy._handle.beekeeper_callbacks import BeekeeperCallbacks
from beekeepy._handle.beekeeper_notification_handler import NotificationHandler
from helpy._communication.universal_notification_server import (
    UniversalNotificationServer,
)

if TYPE_CHECKING:
    from pathlib import Path

    from loguru import Logger

    from schemas.notifications import (
        Error,
        OpeningBeekeeperFailed,
        Status,
    )


class BeekeeperCommon(BeekeeperCallbacks):
    def __init__(self, *args: Any, working_directory: Path, logger: Logger, **kwargs: Any) -> None:
        super().__init__(*args, **kwargs)
        self.__exec = BeekeeperExecutable(working_directory, logger)
        self.__notification: UniversalNotificationServer | None = None

    def __create_notification_server(self) -> UniversalNotificationServer:
        return UniversalNotificationServer(NotificationHandler(self))

    def __wait_till_ready(self) -> None:
        self._http_listening_event.wait(timeout=5.0)

    def _handle_error(self, error: Error) -> None:
        pass

    def _handle_status_change(self, status: Status) -> None:
        pass

    def _handle_opening_beekeeper_failed(self, info: OpeningBeekeeperFailed) -> None:
        return super()._handle_opening_beekeeper_failed(info)

    def run(self) -> helpy.HttpUrl | None:
        self._reset_notification_events()
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
        except TimeoutError:
            self._already_working_beekeeper_event.wait(1.0)
            return self._already_working_beekeeper_http_address
        finally:
            self.close()



    def close(self) -> None:
        assert self.__notification is not None
        self.__exec.close()
        self.__notification.close()
        self.__notification = None

    @property
    def notification_endpoint(self) -> str:
        assert self.__notification is not None
        return f"127.0.0.1:{self.__notification.port}"


class Beekeeper(BeekeeperCommon, helpy.Beekeeper):
    def __init__(self, *args: Any, working_directory: Path, logger: Logger, **kwargs: Any) -> None:
        super().__init__(*args, working_directory=working_directory, logger=logger, **kwargs)

    def run(self) -> None:
        super().run()
        assert self._http_endpoint_from_event is not None
        self.http_endpoint = self._http_endpoint_from_event

    def _get_notification_endpoint(self) -> str:
        return self.notification_endpoint


class AsyncBeekeeper(BeekeeperCommon, helpy.AsyncBeekeeper):
    def __init__(self, *args: Any, working_directory: Path, logger: Logger, **kwargs: Any) -> None:
        super().__init__(*args, working_directory=working_directory, logger=logger, **kwargs)

    def run(self) -> None:
        super().run()
        assert self._http_endpoint_from_event is not None
        self.http_endpoint = self._http_endpoint_from_event

    def _get_notification_endpoint(self) -> str:
        return self.notification_endpoint
