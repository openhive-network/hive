import inspect
from typing import Generator, Iterator, Union

from jsonschema.exceptions import ValidationError


def check_hive_int(checker, instance):
    if checker.is_type(instance, 'integer'):
        return True

    if isinstance(instance, str):
        try:
            int(instance)
            return True
        except ValueError:
            return False
    return False


class HiveInt:
    @staticmethod
    def __validation_errors_generator(validator_name, validator, option_value, instance,
                                      _schema) -> Iterator[ValidationError]:
        def cast_to_int(value, error_message) -> Union[int, ValidationError]:
            try:
                return int(value)
            except ValueError:
                return ValidationError(error_message)

        option_value = cast_to_int(
            option_value,
            error_message=f'Incorrect "{validator_name}" option value, should match Int schema (e.g. 42 or "42").',
        )
        if isinstance(option_value, ValidationError):
            yield option_value
            return

        instance = cast_to_int(
            instance,
            error_message=f'Incorrect instance value, should match Int schema (e.g. 42 or "42").',
        )
        if isinstance(instance, ValidationError):
            yield instance
            return

        # jsonschema validates every option (like: minimum, minItems, etc.) in following way.
        # In package are defined validators, which are generators. Yield validations errors.
        # In below lines generator is created and iterated to validate given option.
        errors: Generator[ValidationError, None, None] = validator.VALIDATORS[validator_name](
            validator, option_value, instance, _schema
        )
        for error in errors:
            yield error
    @staticmethod
    def __create_iterator_of_validation_errors(validator, option_value, instance, _schema) -> Iterator[ValidationError]:
        validator_name = inspect.stack()[1].function
        return HiveInt.__validation_errors_generator(validator_name, validator, option_value, instance, _schema)

    @staticmethod
    def exclusiveMinimum(validator, minimum, instance, _schema) -> Iterator[ValidationError]:
        return HiveInt.__create_iterator_of_validation_errors(validator, minimum, instance, _schema)

    @staticmethod
    def exclusiveMaximum(validator, maximum, instance, _schema) -> Iterator[ValidationError]:
        return HiveInt.__create_iterator_of_validation_errors(validator, maximum, instance, _schema)

    @staticmethod
    def multipleOf(validator, divisor, instance, _schema) -> Iterator[ValidationError]:
        return HiveInt.__create_iterator_of_validation_errors(validator, divisor, instance, _schema)

    @staticmethod
    def maximum(validator, maximum, instance, _schema) -> Iterator[ValidationError]:
        return HiveInt.__create_iterator_of_validation_errors(validator, maximum, instance, _schema)

    @staticmethod
    def minimum(validator, minimum, instance, _schema) -> Iterator[ValidationError]:
        return HiveInt.__create_iterator_of_validation_errors(validator, minimum, instance, _schema)
