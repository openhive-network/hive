from __future__ import annotations

from abc import abstractmethod
from typing import TYPE_CHECKING, Any

import helpy
from beekeepy._executable import BeekeeperExecutable
from beekeepy._handle.beekeeper_callbacks import BeekeeperCallbacks
from beekeepy._handle.beekeeper_notification_handler import NotificationHandler
from beekeepy.settings import Settings
from helpy._communication.universal_notification_server import (
    UniversalNotificationServer,
)
from helpy.exceptions import HelpyError

if TYPE_CHECKING:
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
    def __init__(self, *args: Any, settings: Settings, logger: Logger, **kwargs: Any) -> None:
        super().__init__(*args, settings=settings, **kwargs)
        self.__exec = BeekeeperExecutable(settings, logger)
        self.__notification: UniversalNotificationServer | None = None
        self.__notification_event_handler: NotificationHandler | None = None

    def __create_notification_server(self) -> UniversalNotificationServer:
        assert self.__notification is None
        assert self.__notification_event_handler is None

        self.__notification_event_handler = NotificationHandler(self)
        return UniversalNotificationServer(self.__notification_event_handler, settings=self._get_settings())

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

    def _run(self) -> None:
        self.__notification = self.__create_notification_server()
        settings = self._get_settings()
        settings.notification_endpoint = helpy.HttpUrl(f"127.0.0.1:{self.__notification.run()}", protocol="http")
        settings.http_endpoint = settings.http_endpoint or helpy.HttpUrl("127.0.0.1:0", protocol="http")
        settings.working_directory = self.__exec.woring_dir
        self.__exec.run(
            blocking=False,
            arguments=[
                "--notifications-endpoint",
                settings.notification_endpoint.as_string(with_protocol=False),
                "--webserver-http-endpoint",
                settings.http_endpoint.as_string(with_protocol=False),
                "-d",
                settings.working_directory.as_posix(),
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
        assert (endpoint := self._get_settings().notification_endpoint) is not None
        return endpoint.as_string(with_protocol=False)

    @abstractmethod
    def _get_settings(self) -> Settings:
        ...


class Beekeeper(BeekeeperCommon, helpy.Beekeeper):
    def __init__(self, *args: Any, settings: Settings, logger: Logger, **kwargs: Any) -> None:
        super().__init__(*args, settings=settings, logger=logger, **kwargs)

    def run(self) -> None:
        self._restore_settings()
        self._run()
        self.http_endpoint = self._get_http_endpoint_from_event()

    def _get_notification_endpoint(self) -> str:
        return self.notification_endpoint

    def _get_settings(self) -> Settings:
        assert isinstance(self.settings, Settings)
        return self.settings


class AsyncBeekeeper(BeekeeperCommon, helpy.AsyncBeekeeper):
    def __init__(self, *args: Any, settings: Settings, logger: Logger, **kwargs: Any) -> None:
        super().__init__(*args, settings=settings, logger=logger, **kwargs)

    def run(self) -> None:
        self._restore_settings()
        self._run()
        self.http_endpoint = self._get_http_endpoint_from_event()

    def _get_notification_endpoint(self) -> str:
        return self.notification_endpoint

    def _get_settings(self) -> Settings:
        assert isinstance(self.settings, Settings)
        return self.settings
