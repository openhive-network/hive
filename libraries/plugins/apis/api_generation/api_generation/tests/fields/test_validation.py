from __future__ import annotations

from concurrent.futures import ThreadPoolExecutor
from typing import TYPE_CHECKING, Annotated, Final

import msgspec
import pytest
from hiveio_api import Asset, HiveDateTime, validation
from hiveio_api.fields import user_friendly
from hiveio_api.fields.json_string import JsonString
from hiveio_api.fields.validation import AccountName, AssetHive, HiveInt, LegacyAssetHive
from hiveio_api.validation import SchemaError, decoders, is_matching_model, validate
from hiveio_api.validation.decoders import get_hf26_decoder
from hiveio_api.validation.model import PreconfiguredBaseModel

if TYPE_CHECKING:
    from collections.abc import Callable

    DecHook = Callable[[type, object], object]

THREADS: Final[int] = 16


class Account(PreconfiguredBaseModel, kw_only=True):
    name: AccountName
    balance: AssetHive
    post_count: HiveInt
    memo: str | None = None


class LegacyAccount(PreconfiguredBaseModel, kw_only=True):
    name: AccountName
    balance: LegacyAssetHive


class GetAccountsResponse(PreconfiguredBaseModel, kw_only=True):
    accounts: list[Account]


class Comment(PreconfiguredBaseModel, kw_only=True):
    author: Account | None = None
    extensions: list[object] = msgspec.field(default_factory=list)


Vote = tuple[AccountName, HiveInt]
"""Tuple of items of different types - the error of an item is reported by msgspec with its index."""


def nai_hive(amount: str = "1000") -> dict[str, object]:
    return {"amount": amount, "precision": 3, "nai": "@@000000021"}


def account(**overrides: object) -> dict[str, object]:
    return {"name": "alice", "balance": nai_hive(), "post_count": 3, **overrides}


def test_valid_data_has_no_errors() -> None:
    # ACT
    errors = validate({"accounts": [account()]}, GetAccountsResponse)

    # ASSERT
    assert errors == []


def test_all_errors_are_collected_with_paths() -> None:
    # ARRANGE
    data = {
        "accounts": [
            account(name="Invalid!"),
            account(balance={"amount": "1", "precision": 3, "nai": "@@000000013"}),
            {"name": "bob", "balance": nai_hive()},
        ]
    }

    # ACT
    errors = validate(data, GetAccountsResponse)

    # ASSERT
    paths = {error.path for error in errors}
    assert paths == {"$.accounts[0].name", "$.accounts[1].balance.nai", "$.accounts[2]"}, errors
    assert SchemaError("$.accounts[2]", "Object missing required field `post_count`") in errors


def test_unknown_field_is_reported() -> None:
    # ACT
    errors = validate({"accounts": [account(new_field=1)]}, GetAccountsResponse)

    # ASSERT
    assert errors == [SchemaError("$.accounts[0].new_field", "Object contains unknown field `new_field`")]


def test_all_errors_in_optional_struct_are_reported() -> None:
    # ACT
    errors = validate({"author": account(name="Invalid!", post_count="many")}, Comment)

    # ASSERT
    assert [error.path for error in errors] == ["$.author.name", "$.author.post_count"], errors


@pytest.mark.parametrize(
    ("vote", "expected_path", "expected_message"),
    [(["Invalid!", 5], "$[0]", "Expected `str` matching regex"), (["alice", "many"], "$[1]", "matching regex '^-?")],
)
def test_error_in_tuple_of_items_of_different_types_is_reported_at_the_item(
    vote: list[object], expected_path: str, expected_message: str
) -> None:
    # ACT
    errors = validate(vote, Vote)

    # ASSERT
    assert [error.path for error in errors] == [expected_path], errors
    assert expected_message in errors[0].message


def test_all_errors_in_list_of_annotated_values_are_reported() -> None:
    # ACT
    errors = validate([1, -1, 2, -3], list[Annotated[int, msgspec.Meta(ge=0)]])

    # ASSERT
    assert errors == [SchemaError("$[1]", "Expected `int` >= 0"), SchemaError("$[3]", "Expected `int` >= 0")]


def test_msgspec_reports_the_location_in_the_parsed_format() -> None:
    """`validate` takes locations from messages of msgspec - a change of their format has to fail here."""

    class Model(msgspec.Struct):
        values: list[int]

    # ACT
    with pytest.raises(msgspec.ValidationError) as error:
        msgspec.convert({"values": [1, "x"]}, Model)

    # ASSERT
    assert str(error.value) == "Expected `int`, got `str` - at `$.values[1]`"


def test_values_of_any_json_type_are_kept_as_they_are() -> None:
    # ARRANGE
    extensions: list[object] = [1, "a", None, {"b": [1.5, True]}]

    # ACT & ASSERT
    assert validate({"extensions": extensions}, Comment) == []
    assert Comment.parse_builtins({"extensions": extensions}).extensions == extensions


def test_values_on_the_path_to_an_error_are_not_converted_again(monkeypatch: pytest.MonkeyPatch) -> None:
    # ARRANGE
    data = {"accounts": [account(), account(name="Invalid!"), account()]}
    converted: list[int] = []
    convert = validation._convert

    def spied_convert(value: object, type_: object) -> object:
        converted.append(id(value))
        return convert(value, type_)

    monkeypatch.setattr(validation, "_convert", spied_convert)

    # ACT
    errors = validate(data, GetAccountsResponse)

    # ASSERT
    assert [error.path for error in errors] == ["$.accounts[1].name"]
    assert converted.count(id(data)) == 1
    assert id(data["accounts"]) not in converted
    assert id(data["accounts"][1]) not in converted


def test_fields_with_names_of_other_locations_are_validated_separately() -> None:
    # ARRANGE
    class Dotted(PreconfiguredBaseModel):
        a: Account
        a_name: AccountName = msgspec.field(name="a.name")

    # ACT
    errors = validate({"a": account(), "a.name": "Invalid!"}, Dotted)

    # ASSERT
    assert [error.path for error in errors] == ["$.a.name"], errors


def test_errors_in_dict_values_have_paths() -> None:
    # ACT
    errors = validate({"alice": account(name="Invalid!")}, dict[str, Account])

    # ASSERT
    assert [error.path for error in errors] == ["$.alice.name"]


@pytest.mark.parametrize(
    ("model", "valid_balance", "balance_in_other_form"),
    [(Account, nai_hive(), "1.000 HIVE"), (LegacyAccount, "1.000 HIVE", nai_hive())],
)
def test_assets_are_validated_in_form_given_by_their_type(
    model: type[PreconfiguredBaseModel], valid_balance: object, balance_in_other_form: object
) -> None:
    # ARRANGE
    data = {"name": "alice", "balance": valid_balance, "post_count": 3}
    if model is LegacyAccount:
        del data["post_count"]

    # ACT & ASSERT
    assert validate(data, model) == []
    assert [error.path for error in validate({**data, "balance": balance_in_other_form}, model)] == ["$.balance"]


def test_custom_model_validates_only_its_fields() -> None:
    # ARRANGE
    class OnlyName(PreconfiguredBaseModel, forbid_unknown_fields=False):
        name: AccountName

    data = [account(post_count="not a number")]

    # ACT & ASSERT
    assert validate(data, list[OnlyName]) == []
    assert validate(data, list[Account]) != []


@pytest.mark.parametrize(
    ("value", "field_type", "matches"),
    [
        ("alice", user_friendly.AccountName, True),
        ("Invalid!", user_friendly.AccountName, False),
        (5, user_friendly.AccountName, False),
        ("1.000 HIVE", Asset, True),
        (nai_hive(), Asset, True),
        ("x", Asset, False),
        ({**nai_hive(), "nai": 5}, Asset, False),
        (5, Asset, False),
        ("2016-03-24T16:05:00", HiveDateTime, True),
        ("2016-03-24", HiveDateTime, False),
        (5.0, HiveDateTime, False),
        ('{"a": 1}', JsonString, True),
        ("{not json", JsonString, False),
        (65535, user_friendly.Uint16t, True),
        (65536, user_friendly.Uint16t, False),
        (None, user_friendly.Uint16t, False),
    ],
)
def test_is_matching_model(value: object, field_type: Callable[[object], object], *, matches: bool) -> None:
    # ACT & ASSERT
    assert is_matching_model(value, field_type) is matches


def votes_model(name: str) -> type[PreconfiguredBaseModel]:
    """Model never used before - msgspec builds the information about it on its first use."""
    return type(name, (PreconfiguredBaseModel,), {"__annotations__": {"voter": "AccountName"}})


@pytest.mark.parametrize(
    "use",
    [
        lambda model: validate({"voter": "alice"}, model),
        lambda model: model.parse_builtins({"voter": "alice"}),
        lambda model: get_hf26_decoder(model),
    ],
    ids=["validate", "parse_builtins", "get_hf26_decoder"],
)
def test_first_use_of_type_by_msgspec_is_guarded(
    use: Callable[[type[PreconfiguredBaseModel]], object], monkeypatch: pytest.MonkeyPatch
) -> None:
    # ARRANGE
    model = votes_model("Votes")
    guarded: list[bool] = []
    convert, decoder = msgspec.convert, msgspec.json.Decoder

    def spied_convert(
        obj: object,
        type: object,  # noqa: A002 # as msgspec
        *,
        dec_hook: DecHook | None = None,
        strict: bool = True,
    ) -> object:
        if type is model:
            guarded.append(decoders._preparation_lock.locked())
        return convert(obj, type=type, dec_hook=dec_hook, strict=strict)

    def spied_decoder(type_: object, *, dec_hook: DecHook | None = None) -> object:
        if type_ is model:
            guarded.append(decoders._preparation_lock.locked())
        return decoder(type_, dec_hook=dec_hook)

    monkeypatch.setattr(msgspec, "convert", spied_convert)
    monkeypatch.setattr(msgspec.json, "Decoder", spied_decoder)

    # ACT
    use(model)
    use(model)

    # ASSERT
    assert guarded == [True, False], "only the first use is guarded"


def test_types_used_for_the_first_time_by_many_threads_at_once_are_validated() -> None:
    """Smoke test - the guard of first uses is checked by `test_first_use_of_type_by_msgspec_is_guarded`."""
    # ARRANGE
    models = [votes_model(f"Votes{index}") for index in range(THREADS)]
    data = [{"voter": "alice"} if index % 2 else {"voter": "Invalid!"} for index in range(THREADS)]

    # ACT
    with ThreadPoolExecutor(max_workers=THREADS) as executor:
        results = list(executor.map(validate, data, models))

    # ASSERT
    assert [bool(errors) for errors in results] == [index % 2 == 0 for index in range(THREADS)]
