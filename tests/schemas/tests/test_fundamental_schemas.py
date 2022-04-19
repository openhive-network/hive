from jsonschema.exceptions import ValidationError
import pytest

from schemas.predefined import *


@pytest.mark.parametrize(
    'schema, instance', [
        # String
        (Str(), 'example-string'),
        (Str(minLength=3), '012'),
    ]
)
def test_validation_of_correct_type(schema, instance):
    schema.validate(instance)


@pytest.mark.parametrize(
    'schema, instance', [
        # String
        (Str(), 1),
        (Str(minLength=3), ''),
    ]
)
def test_validation_of_incorrect_type(schema, instance):
    with pytest.raises(ValidationError):
        schema.validate(instance)
