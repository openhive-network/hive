from __future__ import annotations

import json
import sys
import types
from dataclasses import dataclass

import pytest

from tests.generated_package import require_generated_package

require_generated_package()

from hiveio_api import (  # noqa: E402
    Asset,
    HiveioApiBaseError,
    HiveModel,
    UnknownEndpointError,
    register_validation_models,
    validate_json,
    validate_schema,
)
from hiveio_api import _schema_validation  # noqa: E402

from schemas._preconfigured_base_model import PreconfiguredBaseModel  # noqa: E402
from schemas.fields.validation import (  # noqa: E402, TC001  # resolved at runtime by msgspec
    AccountName,
    AssetHive,
    HiveInt,
    LegacyAssetHive,
)

VALIDATION_MODULE = "tests._fake_validation_module"


class Account(PreconfiguredBaseModel, kw_only=True):
    name: AccountName
    balance: AssetHive
    post_count: HiveInt


class LegacyAccount(PreconfiguredBaseModel, kw_only=True):
    name: AccountName
    balance: LegacyAssetHive


class GetAccountsResponse(PreconfiguredBaseModel, kw_only=True):
    accounts: list[Account]


class FakeApi:
    __validation_module__ = VALIDATION_MODULE

    def find_accounts(self) -> None: ...

    def get_legacy_accounts(self) -> None: ...


@dataclass(kw_only=True)
class PublicAccount(HiveModel):
    name: str
    balance: dict[str, object]
    post_count: int


@dataclass(kw_only=True)
class PublicAccountWithAsset(HiveModel):
    name: str
    balance: Asset
    post_count: int


def nai_hive(amount: str = "1000") -> dict[str, object]:
    return {"amount": amount, "precision": 3, "nai": "@@000000021"}


def account(**overrides: object) -> dict[str, object]:
    return {"name": "alice", "balance": nai_hive(), "post_count": 3, **overrides}


@pytest.fixture(autouse=True)
def fake_validation_module(monkeypatch: pytest.MonkeyPatch) -> None:
    module = types.ModuleType(VALIDATION_MODULE)
    module.ENDPOINT_RESULTS = {  # type: ignore[attr-defined]
        "find_accounts": (GetAccountsResponse, False),
        "get_accounts": (Account, True),
        "get_legacy_accounts": (LegacyAccount, True),
    }
    monkeypatch.setitem(sys.modules, VALIDATION_MODULE, module)
    # the registry of validation modules is restored after the test - registered APIs do not leak into other tests
    monkeypatch.setattr(_schema_validation, "_registry", dict(_schema_validation._registry))
    register_validation_models("fake_api", VALIDATION_MODULE)


def test_valid_response_has_no_errors() -> None:
    # ACT & ASSERT
    assert validate_schema({"accounts": [account()]}, "fake_api.find_accounts") == []


def test_errors_are_reported_with_paths() -> None:
    # ARRANGE
    response = {"accounts": [account(name="Invalid!"), {"name": "bob", "balance": nai_hive()}]}

    # ACT
    errors = validate_schema(response, "fake_api.find_accounts")

    # ASSERT
    assert {error.path for error in errors} == {"$.accounts[0].name", "$.accounts[1]"}


def test_legacy_assets_are_validated_by_their_types() -> None:
    # ARRANGE
    legacy_account = {"name": "alice", "balance": "1.000 HIVE"}
    account_with_legacy_balance = account(balance="1.000 HIVE")

    # ACT
    errors = validate_schema([account_with_legacy_balance], "fake_api.get_accounts")

    # ASSERT
    assert validate_schema([legacy_account], "fake_api.get_legacy_accounts") == []
    assert [error.path for error in errors] == ["$[0].balance"]


def test_endpoint_given_as_method() -> None:
    # ACT & ASSERT
    assert validate_schema([{"name": "alice", "balance": "1.000 HIVE"}], FakeApi().get_legacy_accounts) == []
    assert validate_schema(json.dumps({"accounts": [account()]}).encode(), FakeApi.find_accounts) == []


def test_public_models_are_converted_back_to_builtins() -> None:
    # ARRANGE
    response = [PublicAccount(name="Invalid!", balance=nai_hive(), post_count=1)]

    # ACT
    errors = validate_schema(response, "fake_api.get_accounts")

    # ASSERT
    assert [error.path for error in errors] == ["$[0].name"]


def test_public_models_in_dict_values_are_converted_back_to_builtins() -> None:
    # ARRANGE
    response = {"alice": PublicAccount(name="Invalid!", balance=nai_hive(), post_count=1)}

    # ACT
    errors = validate_schema(response, "fake_api.get_accounts", model=dict[str, Account])

    # ASSERT
    assert [error.path for error in errors] == ["$.alice.name"]


def test_json_builtins_are_validated_as_they_are() -> None:
    # ARRANGE
    valid = {"accounts": [account()]}
    invalid = {"accounts": [account(name="Invalid!"), {"name": "bob", "balance": nai_hive()}]}

    # ACT
    errors = validate_json(invalid, "fake_api.find_accounts")

    # ASSERT
    assert validate_json(valid, "fake_api.find_accounts") == []
    assert {error.path for error in errors} == {"$.accounts[0].name", "$.accounts[1]"}


def test_json_validation_does_not_convert_values_of_public_models() -> None:
    # ARRANGE - fields of models are read by keys (`HiveModel.__getitem__`), their values are taken as they are
    response = [PublicAccountWithAsset(name="alice", balance=Asset.hive.satoshi(1000), post_count=1)]

    # ACT & ASSERT
    assert validate_schema(response, "fake_api.get_accounts") == []
    assert validate_json(response, "fake_api.get_accounts") != [], "assets of public models are not JSON builtins"


def test_custom_model_validates_only_its_fields() -> None:
    # ARRANGE
    class OnlyName(PreconfiguredBaseModel, forbid_unknown_fields=False):
        name: AccountName

    response = [account(post_count="not a number")]

    # ACT & ASSERT
    assert validate_schema(response, "fake_api.get_accounts", model=list[OnlyName]) == []
    assert validate_schema(response, "fake_api.get_accounts") != []


def test_unknown_endpoint_raises() -> None:
    # ACT & ASSERT
    with pytest.raises(UnknownEndpointError):
        validate_schema({}, "not_registered_api.method")
    with pytest.raises(UnknownEndpointError):
        validate_schema({}, "fake_api.not_existing_method")


def test_unknown_endpoint_error_is_package_error_not_value_error() -> None:
    # ACT & ASSERT
    with pytest.raises(HiveioApiBaseError) as error:
        validate_schema({}, "not_registered_api.method")
    assert not isinstance(error.value, ValueError)
