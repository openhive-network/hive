from __future__ import annotations

from typing import Final, NamedTuple, cast

import msgspec
import pytest
from hiveio_api.fields.validation import (
    AnyAsset,
    AssetHbd,
    AssetHive,
    AssetHiveOrHbd,
    AssetHiveOrVests,
    AssetVests,
    LegacyAnyAsset,
    LegacyAssetHbd,
    LegacyAssetHive,
    LegacyAssetHiveOrHbd,
    LegacyAssetHiveOrVests,
    LegacyAssetVests,
)

NAI_HIVE = {"amount": "1000", "precision": 3, "nai": "@@000000021"}
NAI_HBD = {"amount": "2000", "precision": 3, "nai": "@@000000013"}
NAI_VESTS = {"amount": "3000000", "precision": 6, "nai": "@@000000037"}
LEGACY_HIVE = "1.000 HIVE"
LEGACY_HBD = "2.000 HBD"
LEGACY_VESTS = "3.000000 VESTS"


class Case(NamedTuple):
    asset_type: object  # structs of the NAI form, `Annotated` strings of the legacy form
    value: object
    value_in_other_form: object


CASES: Final[dict[str, Case]] = {
    "AssetHive": Case(AssetHive, NAI_HIVE, LEGACY_HIVE),
    "AssetHbd": Case(AssetHbd, NAI_HBD, LEGACY_HBD),
    "AssetVests": Case(AssetVests, NAI_VESTS, LEGACY_VESTS),
    "AssetHiveOrHbd": Case(AssetHiveOrHbd, NAI_HBD, LEGACY_HBD),
    "AssetHiveOrVests": Case(AssetHiveOrVests, NAI_VESTS, LEGACY_VESTS),
    "AnyAsset": Case(AnyAsset, NAI_HIVE, LEGACY_HIVE),
    "LegacyAssetHive": Case(LegacyAssetHive, LEGACY_HIVE, NAI_HIVE),
    "LegacyAssetHbd": Case(LegacyAssetHbd, LEGACY_HBD, NAI_HBD),
    "LegacyAssetVests": Case(LegacyAssetVests, LEGACY_VESTS, NAI_VESTS),
    "LegacyAssetHiveOrHbd": Case(LegacyAssetHiveOrHbd, LEGACY_HBD, NAI_HBD),
    "LegacyAssetHiveOrVests": Case(LegacyAssetHiveOrVests, LEGACY_VESTS, NAI_VESTS),
    "LegacyAnyAsset": Case(LegacyAnyAsset, LEGACY_HIVE, NAI_HIVE),
}
ASSET_TYPES = pytest.mark.parametrize("case", CASES.values(), ids=CASES.keys())
AT_THE_ASSET: Final[str] = r" - at `\$\.asset(\.nai|\.precision)?`$"
"""Location of the problem - the legacy form, the NAI or the precision of the NAI form."""


def decode(asset_type: object, value: object) -> object:
    """The asset decoded by msgspec itself - validating types of assets need no hooks."""
    model = msgspec.defstruct("Model", [("asset", cast("type", asset_type))])  # also `Annotated` forms (not types)
    return msgspec.structs.asdict(msgspec.convert({"asset": value}, model))["asset"]


@ASSET_TYPES
def test_asset_is_decoded_from_its_form(case: Case) -> None:
    # ACT
    decoded = decode(case.asset_type, case.value)

    # ASSERT
    assert msgspec.to_builtins(decoded) == case.value


@ASSET_TYPES
def test_asset_is_not_decoded_from_the_other_form(case: Case) -> None:
    # ACT & ASSERT
    with pytest.raises(msgspec.ValidationError, match=AT_THE_ASSET):
        decode(case.asset_type, case.value_in_other_form)


@pytest.mark.parametrize(
    ("asset_type", "value"),
    [
        (AssetHiveOrHbd, NAI_VESTS),
        (AssetHiveOrVests, NAI_HBD),
        (LegacyAssetHiveOrHbd, LEGACY_VESTS),
        (LegacyAssetHiveOrVests, LEGACY_HBD),
        (AssetHive, NAI_HBD),
        (LegacyAssetHive, LEGACY_HBD),
        (AnyAsset, {"amount": "1", "precision": 3, "nai": "@@000000099"}),
        (LegacyAnyAsset, "1.000 STEEM"),
    ],
)
def test_asset_of_other_kind_is_rejected(asset_type: object, value: object) -> None:
    # ACT & ASSERT
    with pytest.raises(msgspec.ValidationError, match=AT_THE_ASSET):
        decode(asset_type, value)


@pytest.mark.parametrize(
    ("asset_type", "value"),
    [
        (AssetHbd, {"amount": "1", "precision": 2, "nai": "@@000000013"}),
        (AssetVests, {"amount": "1", "precision": 3, "nai": "@@000000037"}),
        (LegacyAssetHiveOrVests, "1.000000 HIVE"),
        (LegacyAnyAsset, "1.000 VESTS"),
        (LegacyAssetHbd, "1.00 HBD"),
    ],
)
def test_asset_with_precision_of_other_kind_is_rejected(asset_type: object, value: object) -> None:
    # ACT & ASSERT
    with pytest.raises(msgspec.ValidationError, match=AT_THE_ASSET):
        decode(asset_type, value)


@pytest.mark.parametrize(
    ("asset_type", "value"),
    [(LegacyAnyAsset, "1.000 TESTS"), (LegacyAnyAsset, "2.000 TBD"), (LegacyAssetHive, "1.000 TESTS")],
)
def test_legacy_assets_take_symbols_of_both_networks(asset_type: object, value: str) -> None:
    # ACT & ASSERT
    assert decode(asset_type, value) == value


@pytest.mark.parametrize("amount", ["0", "12", 12, "-1", -1])
def test_amount_of_the_nai_form_is_an_integer(amount: object) -> None:
    # ARRANGE
    value = {"amount": amount, "precision": 3, "nai": "@@000000021"}

    # ACT & ASSERT
    assert msgspec.to_builtins(decode(AssetHive, value)) == value


@pytest.mark.parametrize("value", ["-1.000 HIVE", "-0.000001 VESTS"])
def test_legacy_form_takes_negative_amounts(value: str) -> None:
    # ACT & ASSERT
    assert decode(LegacyAnyAsset, value) == value


@pytest.mark.parametrize("amount", ["1.5", "", "-", 1.5, True, None])
def test_amount_of_the_nai_form_of_other_types_is_rejected(amount: object) -> None:
    # ACT & ASSERT
    with pytest.raises(msgspec.ValidationError, match=r" - at `\$\.asset\.amount`$"):
        decode(AssetHive, {"amount": amount, "precision": 3, "nai": "@@000000021"})


def test_unions_of_the_nai_form_are_base_classes_of_their_members() -> None:
    # ACT & ASSERT
    assert all(issubclass(member, AnyAsset) for member in (AssetHive, AssetHbd, AssetVests))
    assert issubclass(AssetHive, AssetHiveOrHbd)
    assert issubclass(AssetHive, AssetHiveOrVests)
    assert issubclass(AssetHbd, AssetHiveOrHbd)
    assert not issubclass(AssetHbd, AssetHiveOrVests)
    assert issubclass(AssetVests, AssetHiveOrVests)
    assert not issubclass(AssetVests, AssetHiveOrHbd)
