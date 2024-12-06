from __future__ import annotations

from abc import ABC, abstractmethod
from collections.abc import Callable
import contextlib
from copy import deepcopy
import json
import operator
import re
from datetime import datetime
from typing import Annotated, Any, Final, Generic, TypeAlias, TypeVar, get_args

import msgspec

from schemas._preconfigured_base_model import PreconfiguredBaseModel
from schemas.fields.assets._validators import validate_nai, validate_precision
from schemas.fields.assets.asset_info import AssetInfo
from schemas.fields.json_string import JsonString

from typing_extensions import Self



ACCOUNT_NAME_SEGMENT_REGEX: Final[str] = r"[a-z][a-z0-9\-]+[a-z0-9]"
BASE_58_REGEX: Final[str] = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz"

Int16t = Annotated[int, msgspec.Meta(ge=-32768, le=32767)]
DEFAULT_WEIGHT: Final[Int16t] = Int16t(0)

CustomIdType = Annotated[str, msgspec.Meta(max_length=32)]



AccountNameMsgspec = Annotated[str, msgspec.Meta(max_length=16, min_length=3, pattern=rf"^{ACCOUNT_NAME_SEGMENT_REGEX}(?:\.{ACCOUNT_NAME_SEGMENT_REGEX})*$")]

# class AccountNameMsgspec(msgspec.Struct):
#     name: str

#     def __post_init__(self):
#         assert len(self.name) > 3
#         assert len(self.name) < 16
#         assert re.match(
#             rf"^{ACCOUNT_NAME_SEGMENT_REGEX}(?:\.{ACCOUNT_NAME_SEGMENT_REGEX})*$", self.name
#         ), f"Invalid account name: {self.name}"


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

class AssetHiveHF26(msgspec.Struct):
    amount: int
    precision: int
    nai: str

class AssetHiveLegacy(msgspec.Struct):
    value: str

AssetHiveT = TypeVar("AssetHiveT", AssetHiveHF26, AssetHiveLegacy)


class _TransferOperation(OperationMsgspec, Generic[AssetHiveT]):
    __operation_name__ = "transfer"
    __offset__ = 2

    from_: AccountNameMsgspec = msgspec.field(name="from")
    to: AccountNameMsgspec
    amount: AssetHiveT
    memo: str

    def json(self):
        return encode(self)


class TransferOperation(_TransferOperation[AssetHiveHF26]):
    ...


class TransferOperationLegacy(_TransferOperation[AssetHiveLegacy]):
    ...



if __name__ == "__main__":
    operation = TransferOperation(from_=AccountNameMsgspec("alice"), to=AccountNameMsgspec("bobi"), amount=AssetHiveHF26(amount=10, precision=3, nai="@@000000021"), memo="")
    operation_encode = msgspec.json.encode(operation)
    operation_decode = msgspec.json.decode(operation_encode, type=TransferOperation)


    # operation_legacy = TransferOperationLegacy(from_="alice", to="bob", amount=AssetHiveLegacy(value="10000 HIVE"), memo="")

    pass
