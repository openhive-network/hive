import jsonschema

from schemas.__private.custom_checkers import hive_int_checker
from schemas.__private.custom_checkers.hive_int_checker import HiveInt


def validate(schema, instance):
    # It allows to add your own types. To do this you should extend `definitions`,
    # with another dictionary key-value. Where key is the name of the new type,
    # and value is a `checker` function. Checker should return True if validation
    # passes or False when validation fails.
    default_validator = jsonschema.validators.validator_for({})

    # Build a new type checkers
    type_checkers = default_validator.TYPE_CHECKER.redefine_many(
        definitions={
            'hive-int': hive_int_checker.check_hive_int,
        })

    # Build a validator with the new type checkers
    validator = jsonschema.validators.extend(
        default_validator,
        validators={
            'HiveInt.multipleOf': HiveInt.multipleOf,
            'HiveInt.minimum': HiveInt.minimum,
            'HiveInt.maximum': HiveInt.maximum,
            'HiveInt.exclusiveMinimum': HiveInt.exclusiveMinimum,
            'HiveInt.exclusiveMaximum': HiveInt.exclusiveMaximum,
        },
        type_checker=type_checkers,
    )

    # Run the new Validator
    validator(schema=schema).validate(instance)
