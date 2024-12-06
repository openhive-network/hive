from __future__ import annotations

import json
import re
from datetime import datetime
from typing import Annotated, Any, Final, Generic, TypeAlias, TypeVar, get_args

import msgspec

from schemas._preconfigured_base_model import PreconfiguredBaseModel
from schemas.fields.json_string import JsonString

ACCOUNT_NAME_SEGMENT_REGEX: Final[str] = r"[a-z][a-z0-9\-]+[a-z0-9]"
BASE_58_REGEX: Final[str] = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz"

Int16t = Annotated[int, msgspec.Meta(ge=-32768, le=32767)]
DEFAULT_WEIGHT: Final[Int16t] = Int16t(0)

CustomIdType = Annotated[str, msgspec.Meta(max_length=32)]


def AccountNameMsgspec(name: str) -> ValidateAccountName:
    return ValidateAccountName(val=name).val


class ValidateAccountName(msgspec.Struct):
    val: str

    def __post_init__(self):
        assert len(self.val) > 3
        assert len(self.val) < 16
        assert re.match(
            rf"^{ACCOUNT_NAME_SEGMENT_REGEX}(?:\.{ACCOUNT_NAME_SEGMENT_REGEX})*$", self.val
        ), f"Invalid account name: {self.val}"


class OperationMsgspec(msgspec.Struct):
    """Base class for all operations to provide valid json serialization"""

    @classmethod
    def get_name(cls) -> str:
        """
        Get the name of the operation.

        e.g. `transfer` for `TransferOperation`
        """
        return cls.__operation_name__

    @classmethod
    def get_name_with_suffix(cls) -> str:
        """
        Get the name of the operation with the `_operation` suffix.

        e.g. `transfer_operation` for `TransferOperation`
        """
        return f"{cls.get_name()}_operation"

    @classmethod
    def offset(cls) -> int:
        return cls.__offset__


class VoteOperationMsgspec(OperationMsgspec):
    __operation_name__ = "vote"
    __offset__ = 0

    voter: AccountNameMsgspec
    author: AccountNameMsgspec
    permlink: str
    weight: Int16t = DEFAULT_WEIGHT


class HF26Representation(msgspec.Struct):
    type: str
    value: Any


def encode(obj):
    return json.dumps(msgspec.to_builtins(obj))


def decode(msg, type=Any):
    return msgspec.convert(json.loads(msg), type=type)


class SimplyTransactionMsgspec(msgspec.Struct):
    ref_block_num: int
    ref_block_prefix: int
    expiration: datetime
    extensions: list[Any]
    signatures: list[str]
    operations: list[Any] = []

    def add_operation(self, operation: Any) -> None:
        self.operations.append(HF26Representation(type=operation.get_name_with_suffix(), value=operation))

    def json(self):
        return encode(self)


AnyJson: TypeAlias = dict | list | tuple | str | int | float | bool | None

JsonFieldType = TypeVar("JsonFieldType", bound=AnyJson)


class ApplicationOperation(PreconfiguredBaseModel):
    __operation_name__: str

    @classmethod
    def get_name(cls) -> str:
        return cls.__operation_name__

    @classmethod
    def validate(cls, value: Any):
        if isinstance(value, cls):
            return value

        if isinstance(value, str):
            try:
                parsed = json.loads(str(value))
                return cls(**parsed)
            except (ValueError, TypeError) as error:
                raise ValueError(f"Value is not a valid application operation string! Received `{value}`") from error

        raise ValueError(f"Value is not a valid type! Received `{value}` with type `{type(value)}`")


class JsonStringMsgspec(msgspec.Struct, Generic[JsonFieldType]):
    """
    It must be possible to get and set json content, load JsonString value form string and dump JsonString to string.
    JsonString has property value which allows to get and set json content as dict, list, str, int, float, bool or None.
    """

    _value: JsonFieldType

    @classmethod
    def validate(cls, value: Any) -> JsonStringMsgspec[JsonFieldType]:
        if isinstance(value, JsonStringMsgspec):
            value = value.value
            if isinstance(value, str):
                return cls.validate(f'"{value}"')
            return cls.validate(value)

        if isinstance(value, str):
            try:
                parsed = json.loads(value)
                return cls(parsed)
            except (ValueError, TypeError) as error:
                raise ValueError(f"Value is not a valid json string! Received `{value}`") from error
        if isinstance(value, get_args(AnyJson)):
            return cls(value)
        if isinstance(value, PreconfiguredBaseModel):
            return cls(value)  # type: ignore[arg-type]

        raise ValueError(f"Value is not a valid type! Received `{value}` with type `{type(value)}`")

    @property
    def value(self) -> JsonFieldType:
        return self._value

    @value.setter
    def value(self, new_value: JsonFieldType) -> None:
        self._value = new_value

    def serialize(self) -> str:
        """Dumps JsonString with no spaces between keys and values"""
        if isinstance(self._value, ApplicationOperation):
            return self._value.json()
        return json.dumps(self._value, separators=(",", ":"), ensure_ascii=False)

    def __getitem__(self, key: str | int) -> AnyJson:
        self.__check_is_value_index_accessible()
        return self._value[key]  # type: ignore[index]

    def __setitem__(self, key: str | int, value: AnyJson) -> None:
        self.__check_is_value_index_accessible()
        self._value[key] = value  # type: ignore[index]

    def __check_is_value_index_accessible(self) -> None:
        if not isinstance(self._value, dict | list | tuple):
            raise TypeError(
                f"The value in JsonString must be dict, list or tuple use subscript, got: `{type(self._value)}`"
            )

    def __eq__(self, other: object) -> bool:
        if isinstance(other, JsonString):
            return bool(self.value == other.value)
        return bool(self.value == other)


class CustomJsonOperationGeneric(OperationMsgspec):
    __operation_name__ = "custom_json"
    __offset__ = 18

    required_auths: list[AccountNameMsgspec]
    required_posting_auths: list[AccountNameMsgspec]
    id_: CustomIdType = msgspec.field(name="id")
    json_: JsonStringMsgspec[JsonFieldType] = msgspec.field(name="json")


if __name__ == "__main__":
    trx = SimplyTransactionMsgspec(
        ref_block_num=1,
        ref_block_prefix=2605225370,
        expiration=datetime.now(),
        signatures=[
            "1f7f017a2a69b64c968536b64495c9e0a6d0ff29c543131b7d9fc4ecb67e51e028258ce7aedd202ea709f9a1fc1dfe143300b040e3ccef0a66f8b367cdc50f6be5"
        ],
        extensions=[],
    )
    vote_operation = VoteOperationMsgspec(voter="alice", author="bob", permlink="permlink", weight=100)
    trx.add_operation(vote_operation)
    json_trx = trx.json()
    dupa = JsonString({"Andrzej": "Duda", "to": ("sie", "uda")})
    dupamsgspec = JsonStringMsgspec({"Andrzej": "Duda", "to": ("sie", "uda")})
