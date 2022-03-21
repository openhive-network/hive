import pytest

from schemas import validate_message
from schemas.predefined import *


def test_validation_of_correct_type():
    validate_message('example-string', Str())


def test_validation_of_incorrect_type():
    with pytest.raises(AssertionError):
        validate_message(1, Str())
