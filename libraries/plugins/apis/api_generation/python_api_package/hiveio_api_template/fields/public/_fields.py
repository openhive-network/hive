"""
Builtin counterparts of the field types - for public models generated in `hiveio_api`.

Every alias below is named exactly like the field type it stands for (its form of validation models in
`hiveio_api.fields.validation` has the same name), but resolves to plain python types (the form in which the value
appears in raw JSON) - except assets, which resolve to the user-friendly `Asset` of both JSON forms, dates, which are
the user-friendly `HiveDateTime` (an aware `datetime`) itself, and integers of 64 bits (`Int64t`, `Uint64t`, `HiveInt`),
which are `int` also when hived sends them as strings (values not fitting doubles of JSON). Generated public API models
use these aliases, so they carry the semantic name without importing any validation machinery.

Aliases of assets and of integers of 64 bits are `Annotated` - their metadata (`AssetForm`, `IntegerForm`) tells the
JSON form of the value, which their type alone does not (`hiveio_api` reads it with
`typing.get_type_hints(..., include_extras=True)`). Being `Annotated`, they cannot be used with `isinstance`.

This module must stay import-light (no msgspec, no other hiveio_api modules than the import-light `_assets` and
`hiveio_api.fields.hive_datetime`).
"""

# ruff: noqa: UP040
# Aliases intentionally use `TypeAlias` (not the `type` statement) - at runtime they must be the plain types,
# so generated models resolve them to builtins.
from __future__ import annotations

from enum import Enum
from typing import Annotated, TypeAlias

from hiveio_api.fields.hive_datetime import HiveDateTime

from ._assets import Asset

__all__ = [
    "AccountName",
    "AnyAsset",
    "AssetHbd",
    "AssetHive",
    "AssetHiveOrHbd",
    "AssetHiveOrVests",
    "AssetVests",
    "BlockId",
    "CustomIdType",
    "HardforkVersion",
    "Hex",
    "HiveDateTime",
    "HiveInt",
    "Int16t",
    "Int64t",
    "JsonString",
    "LegacyAnyAsset",
    "LegacyAssetHbd",
    "LegacyAssetHive",
    "LegacyAssetHiveOrHbd",
    "LegacyAssetHiveOrVests",
    "LegacyAssetVests",
    "OptionallyEmptyAccountName",
    "Permlink",
    "PublicKey",
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


class AssetForm(Enum):
    """JSON form of the assets of a field - metadata of the aliases of assets."""

    NAI = "nai"
    """HF26 form, e.g. `{"amount": "1000", "precision": 3, "nai": "@@000000021"}`."""
    LEGACY = "legacy"
    """Legacy form, e.g. `"1.000 HIVE"` (condenser_api, bridge)."""


class IntegerForm(Enum):
    """JSON form of the integers of a field - metadata of the aliases of integers of 64 bits."""

    NUMBER_OR_STRING = "number_or_string"
    """A number, or a string when the value does not fit doubles of JSON (hived sends such integers of 64 bits so)."""


# basic
AccountName: TypeAlias = str
OptionallyEmptyAccountName: TypeAlias = str
CustomIdType: TypeAlias = str
Permlink: TypeAlias = str
PublicKey: TypeAlias = str
WitnessUrl: TypeAlias = str

# hex
Hex: TypeAlias = str
Sha256: TypeAlias = str
Signature: TypeAlias = str
TransactionId: TypeAlias = str
BlockId: TypeAlias = str

# integers
Uint8t: TypeAlias = int
Int16t: TypeAlias = int
Uint16t: TypeAlias = int
Uint32t: TypeAlias = int
Int64t: TypeAlias = Annotated[int, IntegerForm.NUMBER_OR_STRING]
Uint64t: TypeAlias = Annotated[int, IntegerForm.NUMBER_OR_STRING]
HiveInt: TypeAlias = Annotated[int, IntegerForm.NUMBER_OR_STRING]

# version
Version: TypeAlias = str
HardforkVersion: TypeAlias = str

# json embedded in string
JsonString: TypeAlias = str

# assets - one user-friendly type of both forms; the metadata tells the form of the field
AssetHive: TypeAlias = Annotated[Asset, AssetForm.NAI]
AssetHbd: TypeAlias = Annotated[Asset, AssetForm.NAI]
AssetVests: TypeAlias = Annotated[Asset, AssetForm.NAI]
AssetHiveOrHbd: TypeAlias = Annotated[Asset, AssetForm.NAI]
AssetHiveOrVests: TypeAlias = Annotated[Asset, AssetForm.NAI]
AnyAsset: TypeAlias = Annotated[Asset, AssetForm.NAI]
LegacyAssetHive: TypeAlias = Annotated[Asset, AssetForm.LEGACY]
LegacyAssetHbd: TypeAlias = Annotated[Asset, AssetForm.LEGACY]
LegacyAssetVests: TypeAlias = Annotated[Asset, AssetForm.LEGACY]
LegacyAssetHiveOrHbd: TypeAlias = Annotated[Asset, AssetForm.LEGACY]
LegacyAssetHiveOrVests: TypeAlias = Annotated[Asset, AssetForm.LEGACY]
LegacyAnyAsset: TypeAlias = Annotated[Asset, AssetForm.LEGACY]
