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

        # Map
        (Map({}), {}),
        (
            Map({'k1': Str(), 'k2': Float()}),
            {'k1': '1', 'k2': 1}
        ),
        (
            # Nested map
            Map({
                'k1': Map({
                    'k2': Map({
                        'k3': Bool()
                    })
                })
            }),
            {'k1': {'k2': {'k3': False}}}
        ),

        # Map - required_keys
        (
            # When optional key is missing.
            Map(
                {
                    'name': Str(),
                    'email': Str(),
                    'telephone': Float()  # Optional
                },
                required_keys=['name', 'email']
            ),
            {
                'name': 'Josh',
                'email': 'josh@josh.com',
                # Note, that 'telephone' is missing
            }
        ),
        (
            # When optional key is present.
            Map(
                {
                    'name': Str(),
                    'email': Str(),
                    'telephone': Float()
                },
                required_keys=['name', 'email']
            ),
            {
                'name': 'Josh',
                'email': 'josh@josh.com',
                'telephone': 3.141593,
            }
        ),

        # Map - allow_additional_properties
        (
            Map(
                {'name': Str()},
                allow_additional_properties=True
            ),
            {
                'name': 'Josh',
                'extra-key': 'extra-key',
            }
        ),
        (
            Map(
                {'name': Str()},
                required_keys=['name'],
                allow_additional_properties=True
            ),
            {
                'name': 'Josh',
                'extra-key': 'extra-key',
            }
        ),

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

        # Map
        (Map({}), 5),
        (Map({'key-0': Str()}), 1),
        (Map({'key-0': Str()}), 'example-string'),
        (Map({'key-0': Str()}), {'key-0': 1}),
        (Map({'key-0': Str()}), {'key-0': '0', 'key-1': '1'}),  # By default, map doesn't allow additional properties

        # Map: required_keys
        (
            # Missing required key
            Map(
                {
                    'name': Str(),
                    'email': Str(),
                    'telephone': Float()
                },
                required_keys=['name', 'email']
            ),
            {
                'name': 'Josh',
                # Note, that 'email' is missing
                'telephone': 3.141593,
            }
        ),

        # Map: allow_additional_properties
        (
            # Test if additional properties are disabled by default
            Map(
                {
                    'name': Str(),
                    'email': Str(),
                    'telephone': Float()
                },
            ),
            {
                'name': 'Josh',
                'email': 'josh@josh.com',
                'telephone': 3.141593,
                'extra-key': 'extra-key'  # This property is additional, wasn't included in schema
            }
        ),

        (
            Map(
                {
                    'name': Str(),
                    'email': Str(),
                    'telephone': {'type': 'string'}  # Should be Str()
                },
            ),
            {
                'name': 3,
                'email': 'josh@josh.com',
                'telephone': '123-45-67'
            }
        ),

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
