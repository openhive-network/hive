from __future__ import annotations

from pathlib import Path  # noqa: TCH003

from pydantic import Field

from beekeepy._executable.defaults import BeekeeperDefaults, ExportKeysWalletParams
from helpy import HttpUrl, WsUrl
from helpy._interfaces.config import Config


def http_webserver_default() -> HttpUrl:
    return HttpUrl("0.0.0.0:0")


class BeekeeperConfig(Config):
    wallet_dir: Path = BeekeeperDefaults.DEFAULT_WALLET_DIR
    unlock_timeout: int = BeekeeperDefaults.DEFAULT_UNLOCK_TIMEOUT
    unlock_interval: int = BeekeeperDefaults.DEFAULT_UNLOCK_INTERVAL
    log_json_rpc: Path | None = BeekeeperDefaults.DEFAULT_LOG_JSON_RPC
    webserver_http_endpoint: HttpUrl | None = Field(default_factory=http_webserver_default)
    webserver_unix_endpoint: HttpUrl | None = None
    webserver_ws_endpoint: WsUrl | None = None
    webserver_ws_deflate: int = 0
    webserver_thread_pool_size: int = 1
    notifications_endpoint: HttpUrl | None = BeekeeperDefaults.DEFAULT_NOTIFICATIONS_ENDPOINT
    backtrace: str = BeekeeperDefaults.DEFAULT_BACKTRACE
    plugin: list[str] = Field(default_factory=lambda: ["json_rpc", "webserver"])
    export_keys_wallet: ExportKeysWalletParams | None = BeekeeperDefaults.DEFAULT_EXPORT_KEYS_WALLET
