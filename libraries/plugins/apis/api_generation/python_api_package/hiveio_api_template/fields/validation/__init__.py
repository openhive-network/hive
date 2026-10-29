"""
Forms of the field types in validation models - checked by msgspec itself, no hooks.

Generators of validation models refer to them by the names of the field types - every alias of
`hiveio_api.fields.public` has its form of the same name here. Types of builtins are `Annotated[<builtin>,
msgspec.Meta(...)]` - the user-friendly field types (`hiveio_api.fields.user_friendly`) are checked by the same forms
when created. Assets are msgspec types of their JSON forms.
"""

from __future__ import annotations

from ._aliases import (
    BlockId,
    HardforkVersion,
    Revision,
    TransactionId,
)
from ._assets import (
    AnyAsset,
    AssetHbd,
    AssetHive,
    AssetHiveOrHbd,
    AssetHiveOrVests,
    AssetNaiAmount,
    AssetVests,
    LegacyAnyAsset,
    LegacyAssetHbd,
    LegacyAssetHive,
    LegacyAssetHiveOrHbd,
    LegacyAssetHiveOrVests,
    LegacyAssetVests,
)
from ._custom import (
    HiveDateTime,
    HiveInt,
    Int64t,
    JsonString,
    Uint64t,
)
from ._fields import (
    AccountName,
    CustomIdType,
    Hex,
    Identifier,
    Int16t,
    NodeType,
    OptionallyEmptyAccountName,
    Permlink,
    PrivateKey,
    PublicKey,
    Sha256,
    Signature,
    Uint8t,
    Uint16t,
    Uint32t,
    Version,
    WitnessUrl,
)

__all__ = [
    "AccountName",
    "AnyAsset",
    "AssetHbd",
    "AssetHive",
    "AssetHiveOrHbd",
    "AssetHiveOrVests",
    "AssetNaiAmount",
    "AssetVests",
    "BlockId",
    "CustomIdType",
    "HardforkVersion",
    "Hex",
    "HiveDateTime",
    "HiveInt",
    "Identifier",
    "Int16t",
    "Int64t",
    "JsonString",
    "LegacyAnyAsset",
    "LegacyAssetHbd",
    "LegacyAssetHive",
    "LegacyAssetHiveOrHbd",
    "LegacyAssetHiveOrVests",
    "LegacyAssetVests",
    "NodeType",
    "OptionallyEmptyAccountName",
    "Permlink",
    "PrivateKey",
    "PublicKey",
    "Revision",
    "Sha256",
    "Signature",
    "TransactionId",
    "Uint8t",
    "Uint16t",
    "Uint32t",
    "Uint64t",
    "Version",
    "WitnessUrl",
]
