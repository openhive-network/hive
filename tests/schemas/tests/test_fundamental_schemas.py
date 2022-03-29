from jsonschema.exceptions import ValidationError
import pytest

from schemas.predefined import *


@pytest.mark.parametrize(
    'schema, instance', [
        # Any
        (Any(), True),
        (Any(), 128),
        (Any(), 'example'),

        # Array
        (Array(), []),
        (Array(), [0, True, 'everything-in-the-array']),
        (Array(Float()), [1.01, 1.02, 1.03]),
        (Array(Int()), ['0', 1, '2', 3]),
        (Array(Int(), Bool()), [False, 1, '1']),
        (Array(Int(enum=['0'])), ['0']),
        (Array(Int(), minItems=1), [0, 1]),
        (Array(Int(), maxItems=3), [0, 1, 2]),
        (Array(Int(), minItems=2, maxItems=3), [0, 1, 2]),

        # ArrayStrict
        (ArrayStrict(Int(), Float(), Bool(), String()), [0, 1.2, True, 'string']),

        # Bool
        (Bool(), True),

        # Float
        (Float(), 3.141593),
        (Float(), 128),  # Ints are also floats

        # Int
        (Int(), 1),
        (Int(), '1'),  # Ints can also be saved as string
        (Int(), -1),
        (Int(), '-1'),

        # Map
        (Map({}), {}),
        (
            Map({'k1': String(), 'k2': Float()}),
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
                    'name': String(),
                    'email': String(),
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
                    'name': String(),
                    'email': String(),
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
                {'name': String()},
                allow_additional_properties=True
            ),
            {
                'name': 'Josh',
                'extra-key': 'extra-key',
            }
        ),
        (
            Map(
                {'name': String()},
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
        (String(), 'example-string'),
        (String(minLength=3), '012'),
    ]
)
def test_validation_of_correct_type(schema, instance):
    schema.validate(instance)


@pytest.mark.parametrize(
    'schema, instance', [
        # Array
        (Array(), {}),
        (Array(Int()), ['its-string-not-int']),
        (Array(Int(enum=[0])), [1]),
        (Array(Int(enum=[0])), [0, 1]),
        (Array(Int(), minItems=2), [0]),  # not-enough-elements-in-the-list, parameter minItems
        (Array(Int(), maxItems=1), [0, 1]),  # too-many-elements-in-the-list, parameter maxItems
        (Array(Int(), minItems=2, maxItems=3), [0]),
        (Array(Int(), minItems=2, maxItems=3), [0, 1, 2, 3]),

        # ArrayStrict
        (ArrayStrict({}), []),
        (ArrayStrict(Int(), Float(), Bool()), [1, 1.1, 'its-not-a-bool']),
        (ArrayStrict(Int(), Float(), Bool()), [1, 1.1, True, 'too-many-elements-in-the-list']),
        (ArrayStrict(Int(), String(), Float()), [1, 'not-enough-elements-in-the-list']),

        # Bool
        (Bool(), 0),
        (Bool(), 1),
        (Bool(), 'False'),
        (Bool(), 'True'),

        # Float
        (Float(), None),
        (Float(), True),
        (Float(), '3.141593'),

        # Int
        (Int(), True),
        (Int(), 3.141593),
        (Int(), 'example-string'),

        # Map
        (Map({}), 5),
        (Map({'key-0': String()}), 1),
        (Map({'key-0': String()}), 'example-string'),
        (Map({'key-0': String()}), {'key-0': 1}),
        (Map({'key-0': String()}), {'key-0': '0', 'key-1': '1'}),  # By default, map doesn't allow additional properties

        # Map required_keys
        (
            # Missing required key.
            Map(
                {
                    'name': String(),
                    'email': String(),
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

        # Map allow_additional_properties
        (
            # Test if additional properties are disabled by default
            Map(
                {
                    'name': String(),
                    'email': String(),
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

        # Null
        (Null(), 0),
        (Null(), ''),

        # String
        (String(), 1),
        (String(minLength=3), ''),
    ]
)
def test_validation_of_incorrect_type(schema, instance):
    with pytest.raises(ValidationError):
        schema.validate(instance)
