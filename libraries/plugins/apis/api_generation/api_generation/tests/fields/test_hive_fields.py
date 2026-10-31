from __future__ import annotations

import copy
import datetime
from typing import Any, Final, cast

import msgspec
import pytest
from hiveio_api.fields.hive_constants import HIVE_TIME_FORMAT
from hiveio_api.fields.hive_datetime import HiveDateTime
from hiveio_api.fields.validation import (
    AccountName,
    AssetHbd,
    AssetHive,
    AssetVests,
    HiveInt,
    LegacyAssetHive,
    PublicKey,
)
from hiveio_api.validation.decoders import get_hf26_decoder
from hiveio_api.validation.encoders import get_hf26_encoder
from hiveio_api.validation.model import PreconfiguredBaseModel

from .hive_tests_constants import ACTIVE, OWNER, POSTING


class AccountNameModel(msgspec.Struct):
    field: AccountName


class HiveDateTimeModel(msgspec.Struct):
    field: HiveDateTime


class Authority(PreconfiguredBaseModel):
    """Hand-written model of the shape of an authority - tuples of Hive types."""

    weight_threshold: HiveInt
    account_auths: list[tuple[AccountName, HiveInt]]
    key_auths: list[tuple[PublicKey, HiveInt]]


class AuthorityModel(PreconfiguredBaseModel):
    field: Authority


class PublicKeyModel(msgspec.Struct):
    field: PublicKey


class AssetHiveLegacyModel(msgspec.Struct):
    field: LegacyAssetHive


@pytest.mark.parametrize(
    "value, expected_message",
    [
        ("definitely too long name", "Expected `str` of length <= 16"),
        ("to", "Expected `str` of length >= 3"),
        (
            "123",
            "Expected `str` matching regex '^[a-z]{1}[a-z0-9\\\\-]+[a-z0-9]{1}(?:\\\\.{1}[a-z]{1}[a-z0-9\\\\-]+[a-z0-9]{1})*$'",
        ),
    ],
)
def test_account_name_incorrect_value(value: str, expected_message: str) -> None:
    # ACT
    account_name = msgspec.json.encode(AccountNameModel(field=value))

    with pytest.raises(Exception) as error:
        get_hf26_decoder(AccountNameModel).decode(account_name)

    # ASSERT
    assert expected_message in str(error.value)


@pytest.mark.parametrize("value", ["alice", "bob", "initminer"])
def test_account_name_correct_value(value: str) -> None:
    # ACT
    instance = AccountNameModel(field=value)

    # ASSERT
    assert instance.field == value


@pytest.mark.parametrize("value", ["incorrect", 12345, "@@0000013"])
def test_asset_nai_hive_field_incorrect_pattern(value: str | int) -> None:
    # ARRANGE
    expected_message: Final[str] = " - at `$.nai`"

    # ACT
    with pytest.raises(Exception) as error:
        msgspec.convert({"amount": 12, "precision": 3, "nai": value}, AssetHive)

    # ASSERT
    assert str(error.value).endswith(expected_message)


@pytest.mark.parametrize("value", ["nai_pattern", "@@0000013"])
def test_asset_nai_hbd_field_incorrect_precision(value: str | int) -> None:
    # ARRANGE
    expected_message: Final[str] = "Expected `int`, got `str` - at `$.precision`"

    # ACT
    with pytest.raises(Exception) as error:
        msgspec.convert({"amount": 10, "precision": value, "nai": "@@000000013"}, AssetHbd)

    # ASSERT
    assert expected_message in str(error.value)


@pytest.mark.parametrize("value", ["nai_pattern", "@@0000013"])
def test_asset_nai_vests_field_incorrect_amount(value: str) -> None:
    # ARRANGE
    expected_message: Final[str] = "Expected `str` matching regex"

    # ACT
    with pytest.raises(Exception) as error:
        msgspec.convert({"amount": value, "precision": 6, "nai": "@@000000037"}, AssetVests)

    # ASSERT
    assert expected_message in str(error.value)


@pytest.mark.parametrize("value, valid", [("1970-01-01T00:00:00", datetime.datetime.fromtimestamp(0, tz=datetime.UTC))])
def test_hive_datetime_field_correct_values(value: str, valid: datetime.datetime) -> None:
    # ARRANGE & ACT
    instance = HiveDateTimeModel(field=HiveDateTime(value))

    # ASSERT
    assert valid == instance.field


@pytest.mark.parametrize("value", ["1970-01-01"])
def test_hive_datetime_field_incorrect_values(value: str | datetime.datetime) -> None:
    # ARRANGE
    expected_message: Final[str] = f"Date must be in format {HIVE_TIME_FORMAT}"

    # ACT
    with pytest.raises(Exception) as error:
        HiveDateTimeModel(field=HiveDateTime(value))

    # ASSERT
    assert expected_message in str(error.value)


@pytest.mark.parametrize("value", [POSTING, ACTIVE, OWNER])
def test_authority_field_correct_values(value: dict[str, Any]) -> None:
    # ARRANGE
    encoded_authority = get_hf26_encoder().encode(AuthorityModel(field=Authority(**value)))

    # ACT
    instance = get_hf26_decoder(AuthorityModel).decode(encoded_authority)

    # ASSERT
    assert isinstance(instance, AuthorityModel)
    key, weight = instance.field.key_auths[0]
    assert (key, weight) == tuple(value["key_auths"][0])
    assert instance.field.weight_threshold == value["weight_threshold"]


@pytest.mark.parametrize(
    "authority, parameter, message",
    [
        (
            POSTING,
            "Not_int",
            "Expected `str` matching regex '^-?",
        ),
        (
            OWNER,
            "Bad_account_name",
            "Expected `str` matching regex '^[a-z]{1}[a-z0-9\\\\-]+[a-z0-9]{1}(?:\\\\.{1}[a-z]{1}[a-z0-9\\\\-]+[a-z0-9]{1})*$'",
        ),
        (
            ACTIVE,
            "SMT@@",
            "Expected `str` matching regex '^(?:STM)[123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz]{7,51}$'",
        ),
    ],
)
def test_authority_field_incorrect_values(authority: dict[str, Any], parameter: str, message: str) -> None:
    # ARRANGE
    # `parametrize` hands over the module-level dicts by reference; mutating them here
    # leaks into `test_authority_field_correct_values`, which is parametrized over the
    # same objects. Work on a deep copy - the nested key_auths/account_auths lists need
    # it, a shallow dict() would still alias them.
    is_posting, is_owner = authority == POSTING, authority == OWNER
    authority = copy.deepcopy(authority)
    if is_posting:
        authority["weight_threshold"] = parameter
    elif is_owner:
        authority["account_auths"].append([parameter, 1])
    else:
        authority["key_auths"][0][0] = parameter
    instance = AuthorityModel(field=Authority(**authority))

    encoder = get_hf26_encoder()
    encoded_authority = encoder.encode(instance)

    decoder = get_hf26_decoder(AuthorityModel)
    # ACT
    with pytest.raises(msgspec.ValidationError) as error:
        decoder.decode(encoded_authority)

    # ASSERT
    assert message in str(error.value)


@pytest.mark.parametrize(
    "value",
    [
        "SMM69zfrFGnZtU3gWFWpQJ6GhND1nz7TJsKBTjcWfebS1JzBEweQy",
        "STM12345",
        "STM1234KASJDzskakxKqwedskldmeokllsdsdbfwsggdaWf123dfs3efa2sga2wdwsfnKSM",
    ],
)
def test_public_key_field_incorrect_values(value: str) -> None:
    # ARRANGE
    expected_message: Final[str] = (
        "Expected `str` matching regex '^(?:STM)[123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz]{7,51}$'"
    )

    instance = PublicKeyModel(field=value)
    public_key_model = msgspec.json.encode(instance)

    decoder = get_hf26_decoder(PublicKeyModel)
    # ACT
    with pytest.raises(Exception) as error:
        decoder.decode(public_key_model)

    # ASSERT
    assert expected_message in str(error.value)


def test_correct_value_asset_hive_legacy() -> None:
    # ACT
    instance = AssetHiveLegacyModel(field="1.000 HIVE")
    asset_hive_legacy_model = msgspec.json.encode(instance)

    decoder = get_hf26_decoder(AssetHiveLegacyModel)

    # ACT
    decoded = cast(AssetHiveLegacyModel, decoder.decode(asset_hive_legacy_model))

    # ASSERT
    assert decoded.field == "1.000 HIVE"


def test_incorrect_value_asset_hive_legacy() -> None:
    # ARRANGE
    expected_message: Final[str] = "Expected `str` matching regex"

    instance = AssetHiveLegacyModel(field="1.000 BAD")
    asset_hive_legacy_model = msgspec.json.encode(instance)

    decoder = get_hf26_decoder(AssetHiveLegacyModel)

    # ACT
    with pytest.raises(Exception) as error:
        decoder.decode(asset_hive_legacy_model)

    # ASSERT
    assert expected_message in str(error.value)
    assert str(error.value).endswith(" - at `$.field`")
