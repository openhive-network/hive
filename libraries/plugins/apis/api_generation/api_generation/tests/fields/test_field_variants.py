from __future__ import annotations

import inspect
import json
import pickle
from typing import TYPE_CHECKING, Annotated, Final, get_args, get_origin

import msgspec
import pytest
from hiveio_api.fields import public, user_friendly, validation
from hiveio_api.fields.public import _fields as public_aliases
from hiveio_api.fields.user_friendly._simple_init_validator import SimpleInitValidator
from hiveio_api.validation.encoders import get_hf26_encoder
from hiveio_api.validation.errors import FieldValidationError

if TYPE_CHECKING:
    from collections.abc import Callable

FIELD_TYPES: Final[set[str]] = set(public_aliases.__all__)
"""Names of the field types - aliases of `hiveio_api.fields.public`."""
ASSETS: Final[set[str]] = {
    name
    for name in FIELD_TYPES
    if get_origin(alias := getattr(public, name)) is Annotated and get_args(alias)[0] is public.Asset
}
TYPES_OF_BUILTINS: Final[list[type[SimpleInitValidator]]] = sorted(
    {
        value
        for _, value in inspect.getmembers(user_friendly, inspect.isclass)
        if issubclass(value, SimpleInitValidator) and value is not SimpleInitValidator
    },
    key=lambda value: value.__name__,
)


def test_every_field_type_has_a_form() -> None:
    # ACT & ASSERT
    assert set(validation.__all__) >= FIELD_TYPES


def test_every_field_type_but_assets_has_a_user_friendly_type() -> None:
    """Assets of both forms are one user-friendly `Asset`."""
    # ACT & ASSERT
    assert set(user_friendly.__all__) >= FIELD_TYPES - ASSETS


@pytest.mark.parametrize("field_type", TYPES_OF_BUILTINS, ids=lambda field_type: field_type.__name__)
def test_user_friendly_type_is_checked_by_the_form_of_its_name(field_type: type[SimpleInitValidator]) -> None:
    # ARRANGE
    form = getattr(validation, field_type.__name__)

    # ACT & ASSERT
    assert field_type._form is form
    assert issubclass(field_type, field_type.covered_type())


@pytest.mark.parametrize("name", ["HiveInt", "Int64t", "Uint64t"])
@pytest.mark.parametrize("value", [2**70, -(2**70)])
def test_user_friendly_integers_of_64_bits_are_not_checked_when_created(name: str, value: int) -> None:
    """Ranges of integers of 64 bits are checked only by validation models."""
    # ACT & ASSERT
    assert getattr(user_friendly, name)(value) == value


def test_form_of_json_string_does_not_check_the_content() -> None:
    # ACT & ASSERT
    assert msgspec.convert("{not json", validation.JsonString) == "{not json"


def is_accepted(create: Callable[[], object]) -> bool:
    try:
        create()
    except msgspec.ValidationError:  # also `FieldValidationError` of field types
        return False
    return True


@pytest.mark.parametrize(
    ("name", "value", "accepted"),
    [
        ("AccountName", "gtg", True),
        ("AccountName", ".", False),
        ("AccountName", "ab", False),
        ("AccountName", "a" * 17, False),
        ("OptionallyEmptyAccountName", "", True),
        ("OptionallyEmptyAccountName", "gtg", True),
        ("OptionallyEmptyAccountName", ".", False),
        ("Signature", "a" * 130, True),
        ("Signature", "a", False),
        ("Uint16t", 65535, True),
        ("Uint16t", 65536, False),
        ("Int16t", -32768, True),
        ("Int16t", -32769, False),
        ("Uint8t", 255, True),
        ("Uint8t", 256, False),
        ("Uint8t", -1, False),
        ("Uint32t", 4294967295, True),
        ("Uint32t", 4294967296, False),
        ("PrivateKey", "5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n", True),
        ("PrivateKey", "5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2", False),
        ("PrivateKey", "0JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n", False),
        ("NodeType", "mirrornet", True),
        ("NodeType", "devnet", False),
        ("Version", "1.27.11", True),
        ("Version", "1.27", False),
        ("Hex", "09afAF", True),
        ("Hex", "0g", False),
        ("Sha256", "a" * 64, True),
        ("Sha256", "a" * 63, False),
        ("Identifier", "a" * 40, True),
        ("Identifier", "a" * 41, False),
        ("WitnessUrl", "a" * 2048, True),
        ("WitnessUrl", "a" * 2049, False),
        ("Permlink", "a" * 256, True),
        ("Permlink", "a" * 257, False),
        ("CustomIdType", "a" * 32, True),
        ("CustomIdType", "a" * 33, False),
    ],
)
def test_forms_and_user_friendly_types_accept_the_same_values(name: str, value: object, *, accepted: bool) -> None:
    # ARRANGE
    form = getattr(validation, name)
    field_type = getattr(user_friendly, name)

    # ACT
    accepted_by_form = is_accepted(lambda: msgspec.convert(value, form))
    accepted_by_field_type = is_accepted(lambda: field_type(value))

    # ASSERT
    assert accepted_by_form is accepted
    assert accepted_by_field_type is accepted


def test_forms_take_values_in_their_json_form() -> None:
    """Validation models check JSON as hived emits it - unlike field types, they do not convert a number in a string."""
    # ACT & ASSERT
    assert user_friendly.Uint16t("65535") == 65535  # noqa: PLR2004
    assert not is_accepted(lambda: msgspec.convert("65535", validation.Uint16t))


@pytest.mark.parametrize(
    ("value", "accepted"),
    [("2022-02-21T02:20:21", True), ("2022-02-21 02:20:21", False), ("2022-02-21T02:20:21Z", False), ("now", False)],
)
def test_form_of_hive_date_time_accepts_the_time_format_of_hive(*, value: str, accepted: bool) -> None:
    # ACT & ASSERT
    assert is_accepted(lambda: msgspec.convert(value, validation.HiveDateTime)) is accepted


def test_user_friendly_types_validate_values_when_created() -> None:
    # ACT & ASSERT
    assert user_friendly.AccountName("gtg") == "gtg"
    with pytest.raises(FieldValidationError, match="is not a valid `AccountName`"):
        user_friendly.AccountName(".")


@pytest.mark.parametrize("value", [user_friendly.AccountName("gtg"), user_friendly.Uint16t(5)])
def test_user_friendly_types_are_pickled_and_dumped_as_their_builtin(value: SimpleInitValidator) -> None:
    # ACT
    unpickled = pickle.loads(pickle.dumps(value))  # data of the test

    # ASSERT
    assert unpickled == value
    assert type(unpickled) is type(value)
    assert json.loads(json.dumps(value)) == value


@pytest.mark.parametrize(
    ("value", "expected"), [(user_friendly.AccountName("gtg"), "gtg"), (user_friendly.Uint16t(5), 5)]
)
def test_user_friendly_types_are_encoded_by_msgspec_as_their_builtin(value: object, expected: str | int) -> None:
    """msgspec does not encode subclasses of builtins itself - the hook takes `to_builtins` of the value."""
    # ACT
    encoded = get_hf26_encoder().encode({"value": value})

    # ASSERT
    assert msgspec.json.decode(encoded) == {"value": expected}
