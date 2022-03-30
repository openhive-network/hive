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
        (ArrayStrict(Int(), Float(), Bool(), Str()), [0, 1.2, True, 'string']),

        # Bool
        (Bool(), True),

        # Date
        (Date(), '1970-01-01T00:00:00'),

        # Float
        (Float(), 3.141593),
        (Float(), 128),  # Ints are also floats

        # Int
        (Int(), 1),
        (Int(), '1'),  # Ints can also be saved as string
        (Int(), -1),
        (Int(), '-1'),
        (Int(multipleOf=2), 4),  # Multiple of 2, so 4, 6, 8, 10 ... 122 its valid
        (Int(multipleOf=2), '4'),
        (Int(multipleOf='2'), 4),
        (Int(multipleOf='2'), '4'),
        (Int(minimum=1), 1),  # instance >= 1
        (Int(minimum=1), '1'),
        (Int(minimum='1'), 1),
        (Int(minimum='1'), '1'),
        (Int(maximum=2), 2),  # instance <= 2
        (Int(maximum=2), '2'),
        (Int(maximum='2'), 2),
        (Int(maximum='2'), '2'),
        (Int(exclusiveMinimum=1), 2),  # instance > 1
        (Int(exclusiveMinimum=1), '2'),
        (Int(exclusiveMinimum='1'), 2),
        (Int(exclusiveMinimum='1'), '2'),
        (Int(exclusiveMaximum=10), 9),  # instance < 10
        (Int(exclusiveMaximum=10), '9'),
        (Int(exclusiveMaximum='10'), 9),
        (Int(exclusiveMaximum='10'), '9'),
        (Int(minimum=1, exclusiveMaximum=10), 9),
        (Int(minimum=1, exclusiveMaximum=10), '9'),
        (Int(minimum=1, exclusiveMaximum='10'), 9),
        (Int(minimum=1, exclusiveMaximum='10'), '9'),
        (Int(minimum='1', exclusiveMaximum=10), 9),
        (Int(minimum='1', exclusiveMaximum=10), '9'),
        (Int(minimum='1', exclusiveMaximum='10'), 9),
        (Int(minimum='1', exclusiveMaximum='10'), '9'),

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
        (ArrayStrict(Int(), Str(), Float()), [1, 'not-enough-elements-in-the-list']),

        # Bool
        (Bool(), 0),
        (Bool(), 1),
        (Bool(), 'False'),
        (Bool(), 'True'),

        # Date
        (Date(), 128),
        (Date(), True),
        (Date(), '1970/01/01T00:00:00'),
        (Date(), '1970-01-01-00:00:00'),
        (Date(), '1970-01-01T00:64:00'),  # Too many minutes
        (Date(), '1970-01-32T00:00:00'),  # Too many days
        (Date(), '1970-13-01T00:00:00'),  # Too many months

        # Float
        (Float(), None),
        (Float(), True),
        (Float(), '3.141593'),

        # Int
        (Int(), True),
        (Int(), 3.141593),
        (Int(), 'example-string'),
        (Int(multipleOf=2), 3),
        (Int(multipleOf=2), '3'),
        (Int(multipleOf='2'), 3),
        (Int(multipleOf='2'), '3'),
        (Int(multipleOf='incorrect-value'), '3'),
        (Int(minimum=2), 1),
        (Int(minimum=2), '1'),
        (Int(minimum='2'), 1),
        (Int(minimum='2'), '1'),
        (Int(minimum='2'), '1'),
        (Int(minimum='incorrect-value'), '1'),
        (Int(maximum=1), 2),
        (Int(maximum=1), '2'),
        (Int(maximum='1'), 2),
        (Int(maximum='1'), '2'),
        (Int(maximum='incorrect-value'), '2'),
        (Int(exclusiveMinimum=1), 1),
        (Int(exclusiveMinimum=1), '1'),
        (Int(exclusiveMinimum='1'), 1),
        (Int(exclusiveMinimum='1'), '1'),
        (Int(exclusiveMinimum='incorrect-value'), '1'),
        (Int(exclusiveMaximum=10), 10),
        (Int(exclusiveMaximum=10), '10'),
        (Int(exclusiveMaximum='10'), 10),
        (Int(exclusiveMaximum='10'), '10'),
        (Int(exclusiveMaximum='incorrect-value'), '10'),
        (Int(minimum=1, exclusiveMaximum=10), 10),
        (Int(minimum=1, exclusiveMaximum=10), '10'),
        (Int(minimum=1, exclusiveMaximum='10'), 10),
        (Int(minimum=1, exclusiveMaximum='10'), '10'),
        (Int(minimum=1, exclusiveMaximum='incorrect-value'), '10'),

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
