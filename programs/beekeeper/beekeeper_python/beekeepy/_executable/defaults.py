from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import ClassVar

from pydantic import BaseModel

from helpy import HttpUrl  # noqa: TCH001


@dataclass
class ExportKeysWalletParams:
    wallet_name: str
    wallet_password: str


class BeekeeperDefaults(BaseModel):
    DEFAULT_BACKTRACE: ClassVar[str] = "yes"
    DEFAULT_DATA_DIR: ClassVar[Path] = Path.cwd()
    DEFAULT_EXPORT_KEYS_WALLET: ClassVar[ExportKeysWalletParams | None] = None
    DEFAULT_LOG_JSON_RPC: ClassVar[Path | None] = None
    DEFAULT_NOTIFICATIONS_ENDPOINT: ClassVar[HttpUrl | None] = None
    DEFAULT_UNLOCK_TIMEOUT: ClassVar[int] = 900
    DEFAULT_UNLOCK_INTERVAL: ClassVar[int] = 500
    DEFAULT_WALLET_DIR: ClassVar[Path] = Path.cwd()
    DEFAULT_WEBSERVER_THREAD_POOL_SIZE: ClassVar[int] = 1
    DEFAULT_WEBSERVER_HTTP_ENDPOINT: ClassVar[HttpUrl | None] = None
