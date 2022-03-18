import pykwalify.errors
import pytest

from schemas.package.schemas.fundamental_schemas import *
from schemas.package.validate_message import validate_message


@pytest.mark.parametrize(
    'correct_message', [
        [1, '1970-01-01T00:00:00', 'string', '1']
    ],
)
def test_seq_order_with_correct_values(correct_message):
    schema = Seq2(
        Int2(),
        Date(),
        Str(),
        Int2(),
        matching='unique',
    )

    validate_message(
        correct_message,
        schema,
    )


@pytest.mark.parametrize(
    'incorrect_message', [
        [1, '1970-01-01T00:00:00', 'string', 'wrong'],
    ],
)
def test_seq_order_with_incorrect_values(incorrect_message):
    schema = Seq2(
        Int2(),
        Date(),
        Str(),
        Int2(),
        matching='unique',
    )
    with pytest.raises(AssertionError):
        validate_message(
            incorrect_message,
            schema,
        )


def test_seq_test_second_arg():
    data = [1, 'st', '1970-01-01T00:00:00', 1]
    schema = Seq2(
        Int2(),
        Str(),
        Date(),
        Int2(),
        matching='unique',
    )
    validate_message(
        data,
        schema
    )
