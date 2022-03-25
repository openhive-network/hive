from jsonschema.exceptions import ValidationError
import pytest

from schemas.predefined import *


@pytest.mark.parametrize(
    'schema, instance', [
        # Any
        (Any(), True),
        (Any(), 128),
        (Any(), 'example'),

        # Bool
        (Bool(), True),

        # Float
        (Float(), 3.141593),
        (Float(), 128),  # Ints are also floats

        # Null
        (Null(), None),

        # String
        (Str(), 'example-string'),
        (Str(minLength=3), '012'),
    ]
)
def test_validation_of_correct_type(schema, instance):
    schema.validate(instance)


@pytest.mark.parametrize(
    'schema, instance', [
        # Bool
        (Bool(), 0),
        (Bool(), 1),
        (Bool(), 'False'),
        (Bool(), 'True'),

        # Float
        (Float(), None),
        (Float(), True),
        (Float(), '3.141593'),

        # Null
        (Null(), 0),
        (Null(), ''),

        # String
        (Str(), 1),
        (Str(minLength=3), ''),
    ]
)
def test_validation_of_incorrect_type(schema, instance):
    with pytest.raises(ValidationError):
        schema.validate(instance)
