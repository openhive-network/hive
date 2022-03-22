import pytest

from schemas.predefined import *
from schemas import validate_message


def test_strict_matching():
    validate_message(
        [True, '1970-01-01T00:00:00', 'string', True],
        Seq(Bool(), Date(), Str(), Bool(), matching='strict'),
    )


@pytest.mark.parametrize(
    'incorrect_message', [
        [True],  # Missing items
        [True, '1970-01-01T00:00:00', 'example-string', False, 1],  # Too many items
        [True, '1970-01-01T00:00:00', False, 'example-string'],  # Wrong order
    ],
)
def test_strict_matching_mismatch(incorrect_message):
    with pytest.raises(AssertionError):
        validate_message(
            incorrect_message,
            Seq(Bool(), Date(), Str(), Bool(), matching='strict'),
        )
