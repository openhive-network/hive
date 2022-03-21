from typing import Any, Dict

import pytest

from schemas import validate_message
from schemas.predefined import *


@pytest.mark.parametrize(
    'correct_value, schema', [
        ('example-string', Str()),
    ]
)
def test_validation_of_correct_type(correct_value: Any, schema: Dict[str, Any]):
    validate_message(correct_value, schema)


@pytest.mark.parametrize(
    'incorrect_value, schema', [
        (1, Str()),
    ]
)
def test_validation_of_incorrect_type(incorrect_value: Any, schema: Dict[str, Any]):
    with pytest.raises(AssertionError):
        validate_message(incorrect_value, schema)
