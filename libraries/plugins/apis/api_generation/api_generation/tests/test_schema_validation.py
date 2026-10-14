from __future__ import annotations

import json
import sys
import types
from dataclasses import dataclass
from typing import TYPE_CHECKING

import pytest

from tests.generated_package import require_generated_package

require_generated_package()

from hiveio_api import (  # noqa: E402
    HiveioApiBaseError,
    HiveModel,
    UnknownEndpointError,
    register_validation_models,
    validate_schema,
)

from schemas._preconfigured_base_model import PreconfiguredBaseModel  # noqa: E402
from schemas.fields.assets import (  # noqa: E402, TC001  # resolved at runtime by msgspec
    AssetHive,
    LegacyAssetHive,
)
from schemas.fields.basic import AccountName  # noqa: E402, TC001  # resolved at runtime by msgspec
from schemas.fields.hive_int import HiveInt  # noqa: E402, TC001  # resolved at runtime by msgspec

if TYPE_CHECKING:
    from collections.abc import Iterator

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


@dataclass(frozen=True, kw_only=True)
class PublicAccount(HiveModel):
    name: str
    balance: dict[str, object]
    post_count: int


def nai_hive(amount: str = "1000") -> dict[str, object]:
    return {"amount": amount, "precision": 3, "nai": "@@000000021"}


def account(**overrides: object) -> dict[str, object]:
    return {"name": "alice", "balance": nai_hive(), "post_count": 3, **overrides}


@pytest.fixture(autouse=True)
def fake_validation_module() -> Iterator[None]:
    module = types.ModuleType(VALIDATION_MODULE)
    module.ENDPOINT_RESULTS = {  # type: ignore[attr-defined]
        "find_accounts": ("GetAccountsResponse", False),
        "get_accounts": ("Account", True),
        "get_legacy_accounts": (LegacyAccount, True),
    }
    module.Account = Account  # type: ignore[attr-defined]
    module.GetAccountsResponse = GetAccountsResponse  # type: ignore[attr-defined]
    sys.modules[VALIDATION_MODULE] = module
    register_validation_models("fake_api", VALIDATION_MODULE)
    yield
    del sys.modules[VALIDATION_MODULE]


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
    legacy = [{"name": "alice", "balance": "1.000 HIVE"}]

    # ACT & ASSERT
    assert validate_schema(legacy, "fake_api.get_legacy_accounts") == []
    assert validate_schema(legacy, "fake_api.get_accounts") != []


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
