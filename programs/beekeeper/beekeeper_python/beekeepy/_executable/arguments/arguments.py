from __future__ import annotations

from abc import ABC, abstractmethod
from pathlib import Path
from typing import TYPE_CHECKING, Any

from pydantic import Field

from schemas._preconfigured_base_model import PreconfiguredBaseModel

if TYPE_CHECKING:
    from typing_extensions import Self


class Arguments(PreconfiguredBaseModel, ABC):
    help_: bool = Field(alias="help", default=False)
    version: bool = False
    dump_config: bool = False

    class Config:
        arbitrary_types_allowed = True

    def __convert_member_name_to_cli_value(self, member_name: str) -> str:
        return member_name.replace("_", "-")

    def __convert_member_value_to_string(self, member_value: int | str | Path | Any) -> str:
        if isinstance(member_value, bool):
            return ""
        if isinstance(member_value, str):
            return member_value
        if isinstance(member_value, int):
            return str(member_value)
        if isinstance(member_value, Path):
            return member_value.as_posix()
        if isinstance(result := self._convert_member_value_to_string_default(member_value=member_value), str):
            return result
        raise TypeError("Invalid type")

    @abstractmethod
    def _convert_member_value_to_string_default(self, member_value: Any) -> str | Any: ...

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

    @classmethod
    def just_get_help(cls) -> Self:
        return cls(help_=True)

    @classmethod
    def just_get_version(cls) -> Self:
        return cls(version=True)

    @classmethod
    def just_dump_config(cls) -> Self:
        return cls(dump_config=True)
