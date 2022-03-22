from typing import Any, Dict

import pytest

from schemas import validate_message
from schemas.predefined import *


@pytest.mark.parametrize(
    'correct_value, schema', [
        # Any
        (True, Any_()),
        (128, Any_()),
        ('example', Any_()),

        # Bool
        (True, Bool()),

        # Date
        ('1970-01-01T00:00:00', Date()),

        # Float
        (3.141593, Float()),
        (128, Float()),  # Ints are also floats
        ('3.141593', Float()),  # Strings are converted to floats

        # Map
        ({'key': 'value'}, Map({'key': Str()})),
        # FIXME: ({}, Map({}, allowempty=True)),

        # None
        ({'empty': None}, Map({'empty': None_()})),

        # Seq
        (['first', 'second', 'third'], Seq(Str())),
        ([], Seq(Str())),

        # Str
        ('example-string', Str()),
    ]
)
def test_validation_of_correct_type(correct_value: Any, schema: Dict[str, Any]):
    validate_message(correct_value, schema)


@pytest.mark.parametrize(
    'incorrect_value, schema', [
        # Bool
        (0, Bool()),
        (1, Bool()),
        ('False', Bool()),
        ('True', Bool()),

        # Date
        (128, Date()),
        ('1970-01-01T00:64:00', Date()),  # Too many minutes

        # Float
        (True, Float()),

        # Map
        (1, Map({'key': Str()})),  # TODO: Use allowempty=True when will be fixed
        ('example-string', Map({'key': Str()})),  # TODO: Use allowempty=True when will be fixed

        # None
        (0, None_()),
        ('', None_()),

        # Seq
        ([1], Seq(Str())),

        # Str
        (1, Str()),
        ([], Str()),
    ]
)
def test_validation_of_incorrect_type(incorrect_value: Any, schema: Dict[str, Any]):
    with pytest.raises(AssertionError):
        validate_message(incorrect_value, schema)
