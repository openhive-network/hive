import pytest

from schemas import validate_message
from schemas.predefined import *


@pytest.mark.parametrize(
    'correct_message', [
        1,
        '2',  # Accepts ints as strings
    ],
)
def test_int_with_correct_value(correct_message):
    validate_message(correct_message, Int())


@pytest.mark.parametrize(
    'incorrect_message', [
        'st',
        False,
        [1],
        ['2'],
    ]
)
def test_int_with_incorrect_value(incorrect_message):
    with pytest.raises(AssertionError):
        validate_message(incorrect_message, Int())


@pytest.mark.parametrize(
    'enum', [
        [1],
        [0, 1],
    ]
)
def test_if_value_matches_with_enum(enum):
    validate_message(1, Int(enum=enum))


@pytest.mark.parametrize(
    'enum', [
        [-1],
        [2],
    ]
)
def test_message_mismatch_with_enumerated_values(enum):
    with pytest.raises(AssertionError):
        validate_message(1, Int(enum=enum))


@pytest.mark.parametrize(
    'incorrect_enum', [
        False,
        None,
        'string',
        1.0,
    ]
)
def test_incorrect_enum_types(incorrect_enum):
    with pytest.raises(AssertionError):
        validate_message(1, Int(enum=[incorrect_enum]))
