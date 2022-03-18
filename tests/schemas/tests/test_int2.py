import pykwalify.errors
import pytest

from schemas.package.schemas.fundamental_schemas import *
from schemas.package.validate_message import validate_message


@pytest.mark.parametrize(
    'correct_message', [1, '2'],
)
def test_hive_int_with_correct_value(correct_message):
    validate_message(correct_message, Int2())


@pytest.mark.parametrize(
    'incorrect_message', ['st', None, False, [1], ['2'], (1,)]
)
def test_hive_int_with_incorrect_value(incorrect_message):
    with pytest.raises(AssertionError):
        validate_message(incorrect_message, Int2())


@pytest.mark.parametrize(
    'correct_enum', [
        [1],
        [0, 1],
    ]
)
def test_if_value_matches_with_enum(correct_enum):
    validate_message(1, Int2(enum=correct_enum))


@pytest.mark.parametrize(
    'incorrect_enum', [2, False, None, -1, 'string']
)
def test_hive_int_enum_option_incorrect_value(incorrect_enum):
    schema = Int2(enum=[incorrect_enum])
    data = 1

    with pytest.raises(AssertionError):
        validate_message(
            data,
            schema,
        )
