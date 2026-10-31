from __future__ import annotations

from typing import Annotated

import msgspec

from hiveio_api.fields.hive_constants import (
    ACCOUNT_NAME_MAX_LENGTH,
    ACCOUNT_NAME_MIN_LENGTH,
    ACCOUNT_NAME_REGEX,
    BASE_58_REGEX,
    HIVE_MAX_WITNESS_URL_LENGTH,
)

__all__ = [
    "AccountName",
    "CustomIdType",
    "Hex",
    "Identifier",
    "Int16t",
    "NodeType",
    "OptionallyEmptyAccountName",
    "Permlink",
    "PrivateKey",
    "PublicKey",
    "Sha256",
    "Signature",
    "Uint8t",
    "Uint16t",
    "Uint32t",
    "Version",
    "WitnessUrl",
]

Uint8t = Annotated[int, msgspec.Meta(ge=0, le=255)]
Int16t = Annotated[int, msgspec.Meta(ge=-32768, le=32767)]
Uint16t = Annotated[int, msgspec.Meta(ge=0, le=65535)]
Uint32t = Annotated[int, msgspec.Meta(ge=0, le=4294967295)]
Hex = Annotated[str, msgspec.Meta(pattern=r"^[0-9a-fA-F]*$")]
Sha256 = Annotated[str, msgspec.Meta(min_length=64, max_length=64)]
Signature = Annotated[str, msgspec.Meta(min_length=130, max_length=130)]
Identifier = Annotated[str, msgspec.Meta(min_length=40, max_length=40)]
AccountName = Annotated[
    str,
    msgspec.Meta(min_length=ACCOUNT_NAME_MIN_LENGTH, max_length=ACCOUNT_NAME_MAX_LENGTH, pattern=ACCOUNT_NAME_REGEX),
]
OptionallyEmptyAccountName = Annotated[
    str, msgspec.Meta(max_length=ACCOUNT_NAME_MAX_LENGTH, pattern=f"{ACCOUNT_NAME_REGEX}|^()$")
]
CustomIdType = Annotated[str, msgspec.Meta(max_length=32)]
NodeType = Annotated[str, msgspec.Meta(pattern=r"^(mainnet|testnet|mirrornet)$")]
Permlink = Annotated[str, msgspec.Meta(max_length=256)]
PublicKey = Annotated[str, msgspec.Meta(pattern=rf"^(?:STM)[{BASE_58_REGEX}]{{7,51}}$")]
PrivateKey = Annotated[str, msgspec.Meta(pattern=rf"^[{BASE_58_REGEX}]{{51}}$")]
WitnessUrl = Annotated[str, msgspec.Meta(max_length=HIVE_MAX_WITNESS_URL_LENGTH)]
Version = Annotated[str, msgspec.Meta(pattern=r"^\d+\.\d+\.\d+$")]
