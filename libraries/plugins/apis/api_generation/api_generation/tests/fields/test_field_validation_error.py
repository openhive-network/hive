from __future__ import annotations

from typing import TYPE_CHECKING

import msgspec
import pytest
from hiveio_api.fields import validation
from hiveio_api.fields.user_friendly import AccountName, HiveDateTime, JsonString, Uint16t
from hiveio_api.validation.decoders import dec_hook_hf26
from hiveio_api.validation.errors import FieldValidationError, SchemaBaseError
from hiveio_api.validation.model import PreconfiguredBaseModel

if TYPE_CHECKING:
    from collections.abc import Callable


class Fields(PreconfiguredBaseModel):
    assets: list[validation.AssetHive] | None = None
    legacy_assets: list[validation.LegacyAssetHive] | None = None
    ints: list[validation.HiveInt] | None = None
    uints: list[validation.Uint64t] | None = None
    names: list[validation.AccountName] | None = None
    dates: list[HiveDateTime] | None = None
    jsons: list[JsonString[object]] | None = None


@pytest.mark.parametrize(
    "create",
    [
        lambda: AccountName("Invalid!"),
        lambda: Uint16t(-1),
        lambda: HiveDateTime("2020-01-01"),
        lambda: HiveDateTime("2016-02-30T00:00:00"),
        lambda: JsonString("{not json"),
    ],
)
def test_field_types_raise_field_validation_error(create: Callable[[], object]) -> None:
    # ACT & ASSERT
    with pytest.raises(FieldValidationError) as error:
        create()
    assert isinstance(error.value, SchemaBaseError)
    assert isinstance(error.value, msgspec.ValidationError)
    assert isinstance(error.value, ValueError)


@pytest.mark.parametrize(
    ("data", "expected_path"),
    [
        ({"assets": [{"amount": "1", "precision": 3, "nai": "@@000000013"}]}, "$.assets[0].nai"),
        ({"legacy_assets": ["1.000 HBD"]}, "$.legacy_assets[0]"),
        ({"legacy_assets": [5]}, "$.legacy_assets[0]"),
        ({"ints": [1, "not a number"]}, "$.ints[1]"),
        ({"uints": [-1]}, "$.uints[0]"),
        ({"names": ["alice", "Invalid!"]}, "$.names[1]"),
        ({"dates": ["2020-01-01"]}, "$.dates[0]"),
        ({"jsons": ["{not json"]}, "$.jsons[0]"),
    ],
)
def test_invalid_field_is_reported_by_msgspec_with_its_location(data: dict[str, object], expected_path: str) -> None:
    # ACT & ASSERT
    with pytest.raises(msgspec.ValidationError) as error:
        msgspec.convert(data, Fields, dec_hook=dec_hook_hf26)
    assert str(error.value).endswith(f" - at `{expected_path}`")
