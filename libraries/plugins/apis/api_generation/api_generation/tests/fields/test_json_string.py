from __future__ import annotations

import json
from typing import Any, Final

import pytest
from hiveio_api.fields.json_string import JsonString
from hiveio_api.fields.validation import AccountName, CustomIdType
from hiveio_api.validation import validate
from hiveio_api.validation.decoders import get_hf26_decoder
from hiveio_api.validation.encoders import get_hf26_encoder
from hiveio_api.validation.model import PreconfiguredBaseModel
from msgspec import field

JSON_STRING_LEVEL3: Final[str] = '"c"'
JSON_STRING_LEVEL2: Final[str] = '{"b": ' + JSON_STRING_LEVEL3 + "}"
JSON_STRING_LEVEL1: Final[str] = '{"a": ' + JSON_STRING_LEVEL2 + "}"
FOLLOW_OPERATION_JSON_STRING: Final[str] = '["follow",{"follower":"abs","following":"xyz","what":["blog"]}]'
NUMBER_STRING: Final[str] = "123"
CUSTOM_JSON_ID: Final[str] = "my_id"

SERIALIZED_OPERATION: Final[str] = r'{"from":"abc","to":"xyz","what":["topic"]}'
JSON_OPERATION_STRING: Final[str] = (
    r'{"required_auths": [], "required_posting_auths": [], "id": "my_id", "json": "'
    + SERIALIZED_OPERATION.replace('"', r"\"")
    + r'"}'
)
SERIALIZED_JSON_STRING: Final[str] = json.dumps(json.loads(JSON_OPERATION_STRING), sort_keys=True)

JSON_STRING_INVALID_TYPE_PATTERN: Final[str] = "is not a valid type"


class CustomJsonOperation(PreconfiguredBaseModel):
    """Model of the shape of `custom_json_operation` - JSON of an application operation embedded in a string."""

    required_auths: list[AccountName]
    required_posting_auths: list[AccountName]
    id_: CustomIdType = field(name="id")
    json_: JsonString[object] = field(name="json")


class SomeModel(PreconfiguredBaseModel):
    """Models are not JSON content of `JsonString`."""


class InvalidCustomType:
    pass


def custom_json_operation() -> dict[str, object]:
    return {"required_auths": [], "required_posting_auths": [], "id": CUSTOM_JSON_ID, "json": SERIALIZED_OPERATION}


def make_full_model(object_: Any) -> Any:
    decoder = get_hf26_decoder(type(object_))
    encoder = get_hf26_encoder()
    encoded = encoder.encode(object_)
    return decoder.decode(encoded)


def test_json_getter_outer() -> None:
    # ARRANGE
    op = CustomJsonOperation(
        required_auths=[],
        required_posting_auths=[],
        id_=CUSTOM_JSON_ID,
        json_=JsonString(JSON_STRING_LEVEL1),
    )

    # ACT
    json_root = op.json_.value

    # ASSERT
    assert json_root == json.loads(JSON_STRING_LEVEL1)


def test_json_getter() -> None:
    # ARRANGE
    op = CustomJsonOperation(
        required_auths=[],
        required_posting_auths=[],
        id_=CUSTOM_JSON_ID,
        json_=JsonString(JSON_STRING_LEVEL1),
    )

    # ACT
    assert isinstance(op.json_.value, dict)
    a = op.json_.value["a"]

    # ASSERT
    assert a == json.loads(JSON_STRING_LEVEL2)


def test_json_getter_inner() -> None:
    # ARRANGE
    op = CustomJsonOperation(
        required_auths=[],
        required_posting_auths=[],
        id_=CUSTOM_JSON_ID,
        json_=JsonString(JSON_STRING_LEVEL1),
    )

    # ACT
    assert isinstance(op.json_.value, dict)
    assert isinstance(op.json_.value["a"], dict)
    b = op.json_.value["a"]["b"]

    # ASSERT
    assert b == json.loads(JSON_STRING_LEVEL3)


@pytest.mark.parametrize("new_value", [123, "abc", ["a", "b", "c"], {"a": 1, "b": 2, "c": 3}])
def test_json_setter_outer(new_value: Any) -> None:
    # ARRANGE
    op = CustomJsonOperation(
        required_auths=[],
        required_posting_auths=[],
        id_=CUSTOM_JSON_ID,
        json_=JsonString(JSON_STRING_LEVEL1),
    )

    # ACT
    op.json_.value = new_value

    # ASSERT
    assert op.json_.value == new_value


def test_json_setter() -> None:
    # ARRANGE
    op = CustomJsonOperation(
        required_auths=[],
        required_posting_auths=[],
        id_=CUSTOM_JSON_ID,
        json_=JsonString(JSON_STRING_LEVEL1),
    )

    new_value: Final[str] = "e"

    # ACT
    assert isinstance(op.json_.value, dict)
    op.json_.value["a"] = new_value

    # ASSERT
    assert op.json_.value["a"] == new_value


def test_json_setter_inner() -> None:
    # ARRANGE
    op = CustomJsonOperation(
        required_auths=[],
        required_posting_auths=[],
        id_=CUSTOM_JSON_ID,
        json_=JsonString(JSON_STRING_LEVEL1),
    )

    new_value: Final[str] = "f"

    # ACT
    assert isinstance(op.json_.value, dict)
    assert isinstance(op.json_.value["a"], dict)
    op.json_.value["a"]["b"] = new_value

    # ASSERT
    assert op.json_.value["a"]["b"] == new_value


def test_json_update() -> None:
    # ARRANGE
    op = CustomJsonOperation(
        required_auths=[], required_posting_auths=[], id_=CUSTOM_JSON_ID, json_=JsonString(NUMBER_STRING)
    )
    update_with: Final[dict[str, str]] = {"x": "y", "z": "t"}

    # ACT
    op.json_.value = {}
    op.json_.value.update(update_with)

    # ASSERT

    # ASSERT
    assert op.json_.value == update_with


def test_json_extend() -> None:
    # ARRANGE
    op = CustomJsonOperation(
        required_auths=[], required_posting_auths=[], id_=CUSTOM_JSON_ID, json_=JsonString(NUMBER_STRING)
    )
    extend_with: Final[list[str]] = ["m", "n", "p"]

    # ACT
    op.json_.value = []
    op.json_.value.extend(extend_with)

    # ASSERT
    assert op.json_.value == extend_with


def test_serialization() -> None:
    # ARRANGE
    op = CustomJsonOperation(
        required_auths=[],
        required_posting_auths=[],
        id_=CUSTOM_JSON_ID,
        json_=JsonString(FOLLOW_OPERATION_JSON_STRING),
    )

    # ACT
    dumps = op.json_.serialize()
    json_value = json.loads(FOLLOW_OPERATION_JSON_STRING)
    minimal_json_string = json.dumps(json_value, separators=(",", ":"))

    # ASSERT
    assert dumps == minimal_json_string


def test_get_by_subscript() -> None:
    # ARRANGE
    op = CustomJsonOperation(
        required_auths=[],
        required_posting_auths=[],
        id_=CUSTOM_JSON_ID,
        json_=JsonString(JSON_STRING_LEVEL1),
    )

    # ACT
    actual_value = op.json_["a"]
    expected_value = json.loads(JSON_STRING_LEVEL2)

    # ASSERT
    assert actual_value == expected_value


def test_set_by_subscript() -> None:
    # ARRANGE
    int_value: Final[int] = 124
    op = CustomJsonOperation(
        required_auths=[],
        required_posting_auths=[],
        id_=CUSTOM_JSON_ID,
        json_=JsonString(JSON_STRING_LEVEL1),
    )

    # ACT
    op.json_["a"] = int_value
    actual_value = op.json_["a"]
    expected_value = int_value

    # ASSERT
    assert actual_value == expected_value


def test_construct_from_int() -> None:
    # ARRANGE
    some_int: Final[int] = 125
    op = CustomJsonOperation(
        required_auths=[], required_posting_auths=[], id_=CUSTOM_JSON_ID, json_=JsonString(some_int)
    )

    update_with: Final[dict[str, str]] = {"x": "y", "z": "t"}

    # ACT
    op.json_.value = {}
    op.json_.value.update(update_with)

    # ASSERT
    assert op.json_.value == update_with


def test_application_operation_parse() -> None:
    # ARRANGE
    # ACT
    json_string = JsonString(SERIALIZED_OPERATION)  # type: ignore[var-annotated]

    # ASSERT
    assert json_string.serialize() == SERIALIZED_OPERATION


@pytest.mark.parametrize("value", [SERIALIZED_OPERATION, "123", ""])
def test_json_form_is_a_string_with_json(value: str) -> None:
    # ACT
    errors = validate({**custom_json_operation(), "json": value}, CustomJsonOperation)

    # ASSERT
    assert errors == []


@pytest.mark.parametrize("value", [{"from": "abc"}, ["follow"], 123, None, "{not json"])
def test_json_in_other_forms_is_rejected_by_validation(value: object) -> None:
    # ACT
    errors = validate({**custom_json_operation(), "json": value}, CustomJsonOperation)

    # ASSERT
    assert [error.path for error in errors] == ["$.json"], errors


def test_json_string_with_application_operation_parse() -> None:
    # ARRANGE
    # ACT
    op = CustomJsonOperation.parse_raw(SERIALIZED_JSON_STRING, get_hf26_decoder)

    # ASSERT
    assert op.json(order="sorted") == SERIALIZED_JSON_STRING


@pytest.mark.parametrize("value", [{"from": "abc"}, ["follow"], 123])
def test_json_form_other_than_a_string_is_not_deserialized(value: object) -> None:
    # ACT & ASSERT
    with pytest.raises(TypeError, match="JSON form of JsonString is a string"):
        JsonString.deserialize(value)


@pytest.mark.parametrize("value", [InvalidCustomType(), SomeModel()])
def test_negative_json_string_with_custom_type(value: object) -> None:
    # ACT
    # ASSERT
    with pytest.raises(ValueError, match=JSON_STRING_INVALID_TYPE_PATTERN):
        JsonString(value)


def test_negative_custom_json_operation_with_invalid_application_operation() -> None:
    # ARRANGE
    error_pattern = "is not a valid type"

    # ACT
    # ASSERT
    with pytest.raises(ValueError, match=error_pattern):
        CustomJsonOperation(
            required_auths=[],
            required_posting_auths=[],
            id_=CUSTOM_JSON_ID,
            json_=JsonString(InvalidCustomType()),
        )


def test_negative_custom_json_operation_with_invalid_json_string() -> None:
    # ARRANGE
    error_pattern = "is not a valid type"

    # ACT
    # ASSERT
    with pytest.raises(ValueError, match=error_pattern):
        CustomJsonOperation(
            required_auths=[],
            required_posting_auths=[],
            id_=CUSTOM_JSON_ID,
            json_=JsonString(JsonString(InvalidCustomType())),
        )


def test_comparison_new_object() -> None:
    # ARRANGE
    op1 = CustomJsonOperation(
        required_auths=[],
        required_posting_auths=[],
        id_=CUSTOM_JSON_ID,
        json_=JsonString(JSON_STRING_LEVEL1),
    )

    # ACT
    # ASSERT
    op2 = CustomJsonOperation(
        required_auths=[],
        required_posting_auths=[],
        id_=CUSTOM_JSON_ID,
        json_=JsonString(JSON_STRING_LEVEL1),
    )
    assert op1 == op2
