from __future__ import annotations

import asyncio
import json
import shutil
import tempfile
from contextlib import contextmanager
from http import HTTPStatus
from pathlib import Path
from time import perf_counter
from typing import TYPE_CHECKING, Any

from pydantic import Field, validator

from .clive_config import settings
from .api import BeekeeperApi
from .command_line_args import (
    BeekeeperCLIArguments,
    ExportKeysWalletParams,
)
from .config import BeekeeperConfig, webserver_default
from .defaults import BeekeeperDefaults
from .exceptions import (
    BeekeeperNon200StatusCodeError,
    BeekeeperNotConfiguredError,
    BeekeeperNotificationServerNotSetError,
    BeekeeperNotMatchingIdJsonRPCError,
    BeekeeperNotRunningError,
    BeekeeperResponseError,
    BeekeeperTokenNotAvailableError,
    BeekeeperUrlNotSetError,
)
from .executable import BeekeeperExecutable
from .notifications import (
    BeekeeperNotificationsServer,
    WalletClosingListener,
)
from .clive_communication import Communication
from .clive_logger import logger
from .clive_url import Url
from .clive_base_model import CliveBaseModel
from schemas.jsonrpc import (
    ExpectResultT,
    JSONRPCRequest,
    JSONRPCResult,
    get_response_model,
)

if TYPE_CHECKING:
    from collections.abc import Callable, Iterator
    from types import TracebackType

    from typing_extensions import Self


ExportedKeys = list[dict[str, str]]


class Beekeeper:
    class ConnectionFileData(CliveBaseModel):
        type_: str = Field(alias="type")
        address: str
        port: int

        @validator("type_")
        @classmethod
        def convert_type(cls, value: str) -> str:
            return value.lower()

    def __init__(
        self,
        *,
        communication: Communication | None = None,
        remote_endpoint: Url | None = None,
        run_in_background: bool = False,
        notify_closing_wallet_name_cb: Callable[[], str] | None = None,
    ) -> None:
        if remote_endpoint:
            settings.set("beekeeper.remote_address", str(remote_endpoint))


        self.__communication = communication or Communication()
        self.__run_in_background = run_in_background
        self.is_running = False
        self.is_starting = False
        self.config = BeekeeperConfig()
        self.__notification_server = BeekeeperNotificationsServer(lambda: self.token, notify_closing_wallet_name_cb)
        self.api = BeekeeperApi(self)
        self.__executable = BeekeeperExecutable(self.config, run_in_background=run_in_background)
        self.__token: str | None = None
        self.__next_time_unlock = perf_counter()

    def __enter__(self) -> Self:
        # So that we can run with Beekeeper().launch(args=)
        if self.is_running:
            return self
        return self.launch()

    def __exit__(self, _: type[Exception] | None, ex: Exception | None, ___: TracebackType | None) -> None:
        if not self.__run_in_background:
            self.close()

    def launch(  # noqa: PLR0913
        self,
        *,
        backtrace: str = BeekeeperDefaults.DEFAULT_BACKTRACE,
        data_dir: Path = BeekeeperDefaults.DEFAULT_DATA_DIR,
        log_json_rpc: Path | None = BeekeeperDefaults.DEFAULT_LOG_JSON_RPC,
        notifications_endpoint: Url | None = BeekeeperDefaults.DEFAULT_NOTIFICATIONS_ENDPOINT,
        unlock_timeout: int = BeekeeperDefaults.DEFAULT_UNLOCK_TIMEOUT,
        wallet_dir: Path = BeekeeperDefaults.DEFAULT_WALLET_DIR,
        webserver_thread_pool_size: int = BeekeeperDefaults.DEFAULT_WEBSERVER_THREAD_POOL_SIZE,
        webserver_http_endpoint: Url | None = BeekeeperDefaults.DEFAULT_WEBSERVER_HTTP_ENDPOINT,
    ) -> Self:
        arguments: BeekeeperCLIArguments = BeekeeperCLIArguments(
            help=False,
            version=False,
            backtrace=backtrace,
            data_dir=data_dir,
            log_json_rpc=log_json_rpc,
            notifications_endpoint=notifications_endpoint,
            unlock_timeout=unlock_timeout,
            wallet_dir=wallet_dir,
            webserver_thread_pool_size=webserver_thread_pool_size,
            webserver_http_endpoint=webserver_http_endpoint,
        )
        self.__start(arguments=arguments)
        self.__set_token()
        assert self.token
        return self

    @property
    def token(self) -> str:
        """
        Get token for current session.

        Raises
        ------
        BeekeeperTokenNotAvailableError: If token was not set yet.
        """
        if self.__token is None:
            raise BeekeeperTokenNotAvailableError
        return self.__token

    def __set_token(self) -> None:
        self.__token = (
            self.api.create_session(
                notifications_endpoint=self.__notification_server.http_endpoint.as_string(with_protocol=False),
                salt=str(id(self)),
            )
        ).token

    @property
    def http_endpoint(self) -> Url:
        if not self.config.webserver_http_endpoint and (remote := self.get_remote_address_from_settings()):
            self.config.webserver_http_endpoint = remote

        if not self.config.webserver_http_endpoint:
            raise BeekeeperUrlNotSetError

        return self.config.webserver_http_endpoint

    @property
    def pid(self) -> int:
        return self.__executable.pid

    @property
    def notification_server_http_endpoint(self) -> Url:
        if self.config.notifications_endpoint is None:
            raise BeekeeperNotificationServerNotSetError
        return self.config.notifications_endpoint

    @contextmanager
    def modified_connection_details(
        self,
        max_attempts: int = Communication.DEFAULT_ATTEMPTS,
        timeout_secs: float = Communication.DEFAULT_TIMEOUT_TOTAL_SECONDS,
        pool_time_secs: float = Communication.DEFAULT_POOL_TIME_SECONDS,
    ) -> Iterator[None]:
        """Allows to temporarily change connection details."""
        with self.__communication.modified_connection_details(max_attempts, timeout_secs, pool_time_secs):
            yield

    @staticmethod
    def get_pid_from_file() -> int:
        return BeekeeperExecutable.get_pid_from_file()

    @staticmethod
    def is_already_running_locally() -> bool:
        return BeekeeperExecutable.is_already_running()

    def _send(
        self, result_model: type[ExpectResultT], endpoint: str, **kwargs: Any
    ) -> JSONRPCResult[ExpectResultT]:  # noqa: ARG002, RUF100
        self.__assert_is_running()

        url = self.http_endpoint.as_string()
        request = JSONRPCRequest(method=endpoint, params=kwargs)

        self.__delay_on_unlock(endpoint)
        response = self.__communication.request(
            url,
            data=request.json(by_alias=True),
            max_attempts=1,  # Beekeeper is not idempotent, so we don't retry e.g. unlock can't be retried to fast.
        )

        response_model = get_response_model(result_model, **response)


        if response_model.id_ != request.id_:
            raise BeekeeperNotMatchingIdJsonRPCError(request, response)

        if not isinstance(response_model, JSONRPCResult):
            raise BeekeeperResponseError(url, request, response)
        return response_model

    def __delay_on_unlock(self, endpoint: str) -> None:
        seconds_to_wait = 0.6
        endpoints_to_wait = ("beekeeper_api.unlock", "beekeeper_api.create_session")

        if endpoint not in endpoints_to_wait:
            return

        while perf_counter() < self.__next_time_unlock:
            logger.debug(f"Waiting for {endpoint} to be available...")
            asyncio.sleep(0.1)

        self.__next_time_unlock = perf_counter() + seconds_to_wait

    def __assert_is_running(self) -> None:
        if self.is_starting:
            return

        if not self.is_running:
            if self.__token is not None:
                self.close()
            raise BeekeeperNotRunningError

    def get_wallet_dir(self) -> Path:
        return self.__executable.get_wallet_dir()

    def close(self) -> None:
        logger.info("Closing Beekeeper...")
        if self.__token:
            self.api.close_session()
            self.__token = None
        self.__close_beekeeper()
        self.is_running = False
        logger.info("Beekeeper closed.")

    def attach_wallet_closing_listener(self, listener: WalletClosingListener) -> None:
        self.__notification_server.attach_wallet_closing_listener(listener)

    def detach_wallet_closing_listener(self, listener: WalletClosingListener) -> None:
        self.__notification_server.detach_wallet_closing_listener(listener)

    def __start(self, *, timeout: float = 5.0, arguments: BeekeeperCLIArguments | None = None) -> None:
        logger.info("Starting Beekeeper...")
        self.is_starting = True
        self.__start_notifications_server()
        if not (remote := self.get_remote_address_from_settings()):
            self.__run_beekeeper(timeout=timeout, arguments=arguments)
        else:
            self.config.webserver_http_endpoint = remote

        assert self.config.webserver_http_endpoint is not None
        self.is_running = True
        self.is_starting = False
        logger.info(f"Beekeeper started on {self.config.webserver_http_endpoint}.")

    def __start_notifications_server(self) -> None:
        address = self.__notification_server.listen()
        self.config.notifications_endpoint = address

    def restart(self) -> None:
        self.close()
        self.launch()

    def __run_beekeeper(self, *, timeout: float = 5.0, arguments: BeekeeperCLIArguments | None = None) -> None:
        self.__executable.run(arguments=arguments)
        self.__wait_for_beekeeper_to_start(timeout)
        self.config.webserver_http_endpoint = self.__notification_server.beekeeper_webserver_http_endpoint

    def __wait_for_beekeeper_to_start(self, timeout: float) -> bool:
        ready_event = self.__notification_server.ready.wait(timeout)
        http_listening_event = self.__notification_server.http_listening_event.wait(timeout)
        return ready_event and http_listening_event

    def __wait_for_beekeeper_report_already_running(self, timeout: float) -> bool:
        return self.__notification_server.opening_beekeeper_failed.wait(timeout)

    def __close_beekeeper(
        self, *, wait_for_deleted_pid: bool = True, close_notifications_server: bool = True
    ) -> None:
        try:
            self.__executable.close(wait_for_deleted_pid=wait_for_deleted_pid)
        finally:
            if close_notifications_server:
                self.__notification_server.close()
                self.config.webserver_http_endpoint = webserver_default()

    @classmethod
    def get_path_from_settings(cls) -> Path | None:
        return BeekeeperExecutable.get_path_from_settings()

    @classmethod
    def get_remote_address_from_settings(cls) -> Url | None:
        raw_address = settings.get("beekeeper.remote_address")
        return Url.parse(raw_address) if raw_address else None

    @classmethod
    def get_remote_address_from_connection_file(cls) -> Url | None:
        connection_file = BeekeeperConfig.get_wallet_dir() / "beekeeper.connection"
        if not connection_file.is_file():
            return None

        connection = cls.ConnectionFileData.parse_file(connection_file)

        return Url(connection.type_, connection.address, connection.port)

    def help(self) -> str:  # noqa: A003
        arguments = BeekeeperCLIArguments(help=True)
        return self.__executable.run_and_get_output(allow_empty_notification_server=True, arguments=arguments)

    def version(self) -> str:
        arguments = BeekeeperCLIArguments(version=True)
        return self.__executable.run_and_get_output(allow_empty_notification_server=True, arguments=arguments)

    def generate_beekeepers_default_config(self) -> BeekeeperConfig:
        with tempfile.TemporaryDirectory() as tmpdirname:
            arguments = BeekeeperCLIArguments(
                backtrace=None,
                data_dir=tmpdirname,
                log_json_rpc=None,
                notifications_endpoint=None,
                unlock_timeout=None,
                wallet_dir=None,
                webserver_thread_pool_size=None,
                webserver_http_endpoint=None,
            )
            self.__executable.run_and_get_output(
                allow_empty_notification_server=True, allow_timeout=True, arguments=arguments
            )

            return BeekeeperConfig.load(Path(tmpdirname) / "config.ini")

    def export_keys_wallet(
        self, *, wallet_name: str, wallet_password: str, extract_to: Path | None = None
    ) -> ExportedKeys:
        with tempfile.TemporaryDirectory() as tmpdirname:
            shutil.copy(self.get_wallet_dir() / f"{wallet_name}.wallet", tmpdirname)
            bk = Beekeeper()
            arguments = BeekeeperCLIArguments(
                export_keys_wallet=ExportKeysWalletParams(wallet_name, wallet_password),
                data_dir=tmpdirname,
            )
            bk.__executable.run_and_get_output(allow_empty_notification_server=True, arguments=arguments)
            """Check if keys has been saved in dumped {wallet_name}.keys file."""
            wallet_path = Path.cwd() / f"{wallet_name}.keys"
            with wallet_path.open() as keys_file:
                if extract_to:
                    shutil.copy(wallet_path, extract_to)
                keys: ExportedKeys = json.load(keys_file)
                return keys
