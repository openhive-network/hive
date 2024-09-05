from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import Any, ClassVar, Literal

from beekeepy._executable.arguments.arguments import Arguments
from helpy import HttpUrl


class BeekeeperArgumentsDefaults:
    DEFAULT_BACKTRACE: ClassVar[Literal["yes", "no"]] = "yes"
    DEFAULT_DATA_DIR: ClassVar[Path] = Path.cwd()
    DEFAULT_EXPORT_KEYS_WALLET: ClassVar[ExportKeysWalletParams | None] = None
    DEFAULT_LOG_JSON_RPC: ClassVar[Path | None] = None
    DEFAULT_NOTIFICATIONS_ENDPOINT: ClassVar[HttpUrl | None] = None
    DEFAULT_UNLOCK_TIMEOUT: ClassVar[int] = 900
    DEFAULT_UNLOCK_INTERVAL: ClassVar[int] = 500
    DEFAULT_WALLET_DIR: ClassVar[Path] = Path.cwd()
    DEFAULT_WEBSERVER_THREAD_POOL_SIZE: ClassVar[int] = 32
    DEFAULT_WEBSERVER_HTTP_ENDPOINT: ClassVar[HttpUrl | None] = None

@dataclass
class ExportKeysWalletParams:
    wallet_name: str
    wallet_password: str


class BeekeeperArguments(Arguments):
    backtrace: Literal["yes", "no"] | None = BeekeeperArgumentsDefaults.DEFAULT_BACKTRACE
    data_dir: Path = BeekeeperArgumentsDefaults.DEFAULT_DATA_DIR
    export_keys_wallet: ExportKeysWalletParams | None = BeekeeperArgumentsDefaults.DEFAULT_EXPORT_KEYS_WALLET
    log_json_rpc: Path | None = BeekeeperArgumentsDefaults.DEFAULT_LOG_JSON_RPC
    notifications_endpoint: HttpUrl | None = BeekeeperArgumentsDefaults.DEFAULT_NOTIFICATIONS_ENDPOINT
    unlock_timeout: int | None = BeekeeperArgumentsDefaults.DEFAULT_UNLOCK_TIMEOUT
    wallet_dir: Path | None = BeekeeperArgumentsDefaults.DEFAULT_WALLET_DIR
    webserver_thread_pool_size: int | None = BeekeeperArgumentsDefaults.DEFAULT_WEBSERVER_THREAD_POOL_SIZE
    webserver_http_endpoint: HttpUrl | None = BeekeeperArgumentsDefaults.DEFAULT_WEBSERVER_HTTP_ENDPOINT

    def _convert_member_value_to_string_default(self, member_value: Any) -> str | Any:
        if isinstance(member_value, HttpUrl):
            return member_value.as_string(with_protocol=False)
        if isinstance(member_value, ExportKeysWalletParams):
            return f'["{member_value.wallet_name}","{member_value.wallet_password}"]'
        return member_value
