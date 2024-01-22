from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

from pydantic import BaseModel, Field

from .defaults import BeekeeperDefaults
from .clive_url import Url


@dataclass
class ExportKeysWalletParams:
    wallet_name: str
    wallet_password: str


class BeekeeperCLIArguments(BaseModel):
    help_: bool = Field(alias="help", default=False)
    version: bool = False
    backtrace: str | None = BeekeeperDefaults.DEFAULT_BACKTRACE
    data_dir: Path = BeekeeperDefaults.DEFAULT_DATA_DIR
    export_keys_wallet: ExportKeysWalletParams | None = BeekeeperDefaults.DEFAULT_EXPORT_KEYS_WALLET
    log_json_rpc: Path | None = BeekeeperDefaults.DEFAULT_LOG_JSON_RPC
    notifications_endpoint: Url | None = BeekeeperDefaults.DEFAULT_NOTIFICATIONS_ENDPOINT
    unlock_timeout: int | None = BeekeeperDefaults.DEFAULT_UNLOCK_TIMEOUT
    wallet_dir: Path | None = BeekeeperDefaults.DEFAULT_WALLET_DIR
    webserver_thread_pool_size: int | None = BeekeeperDefaults.DEFAULT_WEBSERVER_THREAD_POOL_SIZE
    webserver_http_endpoint: Url | None = BeekeeperDefaults.DEFAULT_WEBSERVER_HTTP_ENDPOINT

    def __convert_member_name_to_cli_value(self, member_name: str) -> str:
        return member_name.replace("_", "-")

    def __convert_member_value_to_string(self, member_value: int | str | Path | Url | ExportKeysWalletParams) -> str:
        if isinstance(member_value, bool):
            return ""
        if isinstance(member_value, str):
            return member_value
        if isinstance(member_value, int):
            return str(member_value)
        if isinstance(member_value, Path):
            return member_value.as_posix()
        if isinstance(member_value, Url):
            return member_value.as_string(with_protocol=False)
        if isinstance(member_value, ExportKeysWalletParams):
            return f'["{member_value.wallet_name}","{member_value.wallet_password}"]'
        raise TypeError("Invalid type")

    def __prepare_arguments(self, pattern: str) -> list[str]:
        data = self.dict(by_alias=True, exclude_none=True, exclude_unset=True, exclude_defaults=True)
        cli_arguments: list[str] = []
        for k, v in data.items():
            cli_arguments.append(pattern.format(self.__convert_member_name_to_cli_value(k)))
            cli_arguments.append(self.__convert_member_value_to_string(v))
        return cli_arguments

    def process(self, *, with_prefix: bool = True) -> list[str]:
        pattern = "--{0}" if with_prefix else "{0}"
        return self.__prepare_arguments(pattern)
