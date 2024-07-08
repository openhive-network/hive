from __future__ import annotations

import asyncio
from abc import abstractmethod
from concurrent.futures import ThreadPoolExecutor
from typing import TYPE_CHECKING, Any, TypeVar, cast

import helpy
from beekeepy._executable import BeekeeperExecutable
from beekeepy._handle.beekeeper_callbacks import BeekeeperCallbacks
from beekeepy._handle.beekeeper_notification_handler import NotificationHandler
from beekeepy.settings import Settings
from helpy import ContextSync
from helpy._communication.universal_notification_server import (
    UniversalNotificationServer,
)
from helpy.exceptions import HelpyError

if TYPE_CHECKING:
    from pathlib import Path

    from loguru import Logger

    from beekeepy._executable.beekeeper_config import BeekeeperConfig
    from beekeepy._handle.callbacks_protocol import AsyncWalletLocked, SyncWalletLocked
    from helpy import KeyPair
    from schemas.notifications import (
        AttemptClosingWallets,
        Error,
        Notification,
        OpeningBeekeeperFailed,
        Status,
        WebserverListening,
    )


class BeekeeperAlreadyRunningError(HelpyError):
    def __init__(self, *args: object, address: helpy.HttpUrl, pid: int) -> None:
        super().__init__(*args)
        self.address = address
        self.pid = pid


class BeekeeperIsNotRunningError(HelpyError):
    pass


EnterReturnT = TypeVar("EnterReturnT", bound=helpy.Beekeeper | helpy.AsyncBeekeeper)


class BeekeeperCommon(BeekeeperCallbacks, ContextSync[EnterReturnT]):
    def __init__(self, *args: Any, settings: Settings, logger: Logger, **kwargs: Any) -> None:
        super().__init__(*args, settings=settings, **kwargs)
        self.__exec = BeekeeperExecutable(settings, logger)
        self.__notification: UniversalNotificationServer | None = None
        self.__notification_event_handler: NotificationHandler | None = None

    @property
    def pid(self) -> int:
        if self.__exec is None or not self.__exec.is_running():
            raise BeekeeperIsNotRunningError
        return self.__exec.pid

    @property
    def notification_endpoint(self) -> str:
        assert (endpoint := self._get_settings().notification_endpoint) is not None
        return endpoint.as_string(with_protocol=False)

    @property
    def config(self) -> BeekeeperConfig:
        return self.__exec.config

    @property
    def is_running(self) -> bool:
        return self.__exec is not None and self.__exec.is_running()

    def __create_notification_server(self) -> UniversalNotificationServer:
        assert self.__notification is None
        assert self.__notification_event_handler is None

        self.__notification_event_handler = NotificationHandler(self)
        return UniversalNotificationServer(
            self.__notification_event_handler, notification_endpoint=self._get_settings().notification_endpoint
        )

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
                assert (pid := self.__notification_event_handler.already_working_beekeeper_pid) is not None
                raise BeekeeperAlreadyRunningError(address=addr, pid=pid) from err

    def _handle_error(self, error: Error) -> None:
        pass

    def _handle_status_change(self, status: Status) -> None:
        pass

    def _handle_opening_beekeeper_failed(self, info: OpeningBeekeeperFailed) -> None:
        return super()._handle_opening_beekeeper_failed(info)

    def _run(self, settings: Settings) -> None:
        self.__notification = self.__create_notification_server()
        settings.notification_endpoint = helpy.HttpUrl(f"127.0.0.1:{self.__notification.run()}", protocol="http")
        settings.http_endpoint = settings.http_endpoint or helpy.HttpUrl("127.0.0.1:0", protocol="http")
        settings.working_directory = self.__exec.working_directory
        self._run_application(settings=settings)
        try:
            self.__wait_till_ready()
        except BeekeeperAlreadyRunningError:
            self.close()
            raise

    def _run_application(self, settings: Settings) -> None:
        assert settings.notification_endpoint is not None
        assert settings.http_endpoint is not None
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

    def close(self) -> None:
        self._close_application()
        self.__destroy_notification_server()

    def _close_application(self) -> None:
        if self.__exec.is_running():
            self.__exec.close()

    def _http_webserver_ready(self, notification: Notification[WebserverListening]) -> None:
        """It is convered by _get_http_endpoint_from_event."""

    def _get_http_endpoint_from_event(self) -> helpy.HttpUrl:
        assert self.__notification_event_handler is not None
        # <###> if you get exception from here, and have consistent way of reproduce please report <###>
        # make sure you didn't forget to call beekeeper.run() methode
        assert (addr := self.__notification_event_handler.http_endpoint_from_event) is not None
        return addr

    def export_keys_wallet(
        self, wallet_name: str, wallet_password: str, extract_to: Path | None = None
    ) -> list[KeyPair]:
        return self.__exec.export_keys_wallet(
            wallet_name=wallet_name, wallet_password=wallet_password, extract_to=extract_to
        )

    @abstractmethod
    def run(self) -> None:
        ...

    def restart(self) -> None:
        self.close()
        self.run()

    @abstractmethod
    def _get_settings(self) -> Settings:
        ...

    def _enter(self) -> EnterReturnT:
        self.run()
        return cast(EnterReturnT, self)

    def _finally(self) -> None:
        self.close()


class Beekeeper(BeekeeperCommon["Beekeeper"], helpy.Beekeeper):
    def __init__(self, *args: Any, settings: Settings, logger: Logger, **kwargs: Any) -> None:
        super().__init__(*args, settings=settings, logger=logger, **kwargs)
        self.__wallet_close_callbacks: list[SyncWalletLocked] = []

    def run(self) -> None:
        self._clear_session_token()
        with self.update_settings() as settings:
            self._run(settings=cast(Settings, settings))
        self.http_endpoint = self._get_http_endpoint_from_event()

    def _get_notification_endpoint(self) -> str:
        return self.notification_endpoint

    def _get_settings(self) -> Settings:
        assert isinstance(self.settings, Settings)
        return self.settings

    def register_wallet_close_callback(self, callback: SyncWalletLocked) -> None:
        self.__wallet_close_callbacks.append(callback)

    def _handle_wallets_closed(self, note: AttemptClosingWallets) -> None:
        wallet_names = [w.name for w in note.wallets]
        for callback in self.__wallet_close_callbacks:
            callback(wallet_names)

    @property
    def settings(self) -> Settings:
        return cast(Settings, super().settings)


class AsyncBeekeeper(BeekeeperCommon["AsyncBeekeeper"], helpy.AsyncBeekeeper):
    def __init__(self, *args: Any, settings: Settings, logger: Logger, **kwargs: Any) -> None:
        super().__init__(*args, settings=settings, logger=logger, **kwargs)
        self.__wallet_close_callbacks: list[AsyncWalletLocked] = []

    def run(self) -> None:
        self._clear_session_token()
        with self.update_settings() as settings:
            self._run(settings=cast(Settings, settings))
        self.http_endpoint = self._get_http_endpoint_from_event()

    def _get_notification_endpoint(self) -> str:
        return self.notification_endpoint

    def _get_settings(self) -> Settings:
        assert isinstance(self.settings, Settings)
        return self.settings

    def register_wallet_close_callback(self, callback: AsyncWalletLocked) -> None:
        self.__wallet_close_callbacks.append(callback)

    def _handle_wallets_closed(self, note: AttemptClosingWallets) -> None:
        wallet_names = [w.name for w in note.wallets]
        with ThreadPoolExecutor(max_workers=1) as executor:
            for callback in self.__wallet_close_callbacks:
                executor.submit(asyncio.run, callback(wallet_names)).exception()  # type: ignore[arg-type]

    @property
    def settings(self) -> Settings:
        return cast(Settings, super().settings)


class SyncRemoteBeekeeper(Beekeeper):
    def _run_application(self, settings: Settings) -> None:
        pass

    def _close_application(self) -> None:
        pass

    def _get_http_endpoint_from_event(self) -> helpy.HttpUrl:
        assert self.settings.http_endpoint is not None
        return self.settings.http_endpoint


class AsyncRemoteBeekeeper(AsyncBeekeeper):
    def _run_application(self, settings: Settings) -> None:
        pass

    def _close_application(self) -> None:
        pass

    def _get_http_endpoint_from_event(self) -> helpy.HttpUrl:
        assert self.settings.http_endpoint is not None
        return self.settings.http_endpoint
