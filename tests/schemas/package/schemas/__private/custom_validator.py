import jsonschema


def validate(schema, instance):
    # It allows to add your own types. To do this you should extend `definitions`,
    # with another dictionary key-value. Where key is the name of the new type,
    # and value is a `checker` function. Checker should return True if validation
    # passes or False when validation fails.
    default_validator = jsonschema.validators.validator_for({})

    # Build a new type checkers
    custom_types = default_validator.TYPE_CHECKER.redefine_many(
        definitions={
        })

    # Build a validator with the new type checkers
    validator = jsonschema.validators.extend(default_validator, type_checker=custom_types)

    # Run the new Validator
    validator(schema=schema).validate(instance)
