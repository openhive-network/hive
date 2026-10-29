from __future__ import annotations

import json

import msgspec
import pytest
from hiveio_api.fields import user_friendly
from hiveio_api.fields.validation import AccountName  # the pattern of AccountName validated by msgspec
from hiveio_api.validation.errors import FieldValidationError


# Decoder class based on msgspec
class Account(msgspec.Struct):
    name: AccountName


VALID_ACCOUNT_NAMES = pytest.mark.parametrize(
    "valid_account_name",
    [
        "abc",
        "abc123",
        "a1-b2",
        "abc.def",
        "a-b-c",
        "abc.def12",
        "abc1-def2",
        "xyz.abc",
        "my-account",
        "z1-x9",
        "abc.def-ghi",
        "abc123.def456",
        "xyz-a",
        "test.test123",
        "a1b2c3",
        "good-name",
        "name.with-dots",
        "valid.name1",
        "short",
        "longname123",
        "alice",
        "bob",
    ],
)
INVALID_ACCOUNT_NAMES = pytest.mark.parametrize(
    "invalid_account_name",
    [
        "ab",  # Too short
        "a" * 17,  # Too long
        "a.b.c.d.e.f.g",  # Too short segments
        "abc.",  # Dot at the end
        ".abc",  # Dot at the beginning
        "abc..def",  # Double dots
        "abc:.def",  # Colon is not allowed
        "123abc",  # Does not start with a letter
        "abc123!",  # Exclamation mark is not allowed
        "abc$",  # Dollar sign is not allowed
        "abc.def.",  # Dot at the end after a segment
        "a-1-",  # Dash at the end
        "-abc",  # Dash at the beginning
        "abc..",  # Double dots
        "ABC",  # Uppercase letters are not allowed
        "a.b.c.d.e",  # Too many segments
        "3ab",  # Starts with number
        "ab-",  # Dash at the end
        "abc.",  # Dot at the end
    ],
)


@VALID_ACCOUNT_NAMES
def test_valid_account_names(valid_account_name: str) -> None:
    # ARRANGE
    data = {"name": valid_account_name}

    # ACT
    encoded = msgspec.json.encode(data)
    decoded = msgspec.json.decode(encoded, type=Account)

    # ASSERT
    assert decoded.name == valid_account_name


@INVALID_ACCOUNT_NAMES
def test_invalid_account_names(invalid_account_name: str) -> None:
    # ARRANGE
    data = {"name": invalid_account_name}

    # ACT & ASSERT
    encoded = msgspec.json.encode(data)
    with pytest.raises(msgspec.ValidationError):
        msgspec.json.decode(encoded, type=Account)


@VALID_ACCOUNT_NAMES
def test_account_name_is_created_of_valid_name_and_serialized_as_string(valid_account_name: str) -> None:
    # ACT
    name = user_friendly.AccountName(valid_account_name)

    # ASSERT
    assert name == valid_account_name
    assert json.dumps({"name": name}) == json.dumps({"name": valid_account_name})


@INVALID_ACCOUNT_NAMES
def test_account_name_is_not_created_of_invalid_name(invalid_account_name: str) -> None:
    # ACT & ASSERT
    with pytest.raises(FieldValidationError):
        user_friendly.AccountName(invalid_account_name)
