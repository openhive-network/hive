from abc import ABC, abstractmethod
import pprint
from typing import Any, Dict

import jsonschema

from jsonschema.exceptions import ValidationError


class Schema(ABC):
    def __init__(self, **options: Any):
        self._options = options

    def __repr__(self) -> str:
        return str(self._create_schema())

    def validate(self, instance) -> None:
        jsonschema.validate(
            instance=instance,
            schema=self._create_schema(),
        )

    @abstractmethod
    def _create_core_of_schema(self) -> Dict[str, Any]:
        """
        This function create the core of the schema. Core is not a complete schema, it can't be validated.
        To pass validation, _create_schema adds basic schema parts and optional parameters (e.g 'enum', 'minItems'),
        creating a complete schema prepared for validation.
        """
        pass

    def _create_schema(self) -> Dict[str, Any]:
        return {**self._create_core_of_schema(), **self._options}

    @staticmethod
    def _assert_that_schema_has_correct_type(schema):
        if not isinstance(schema, Schema):
            raise ValidationError(f'Passed schema has invalid format:\n'
                                  f'{pprint.pformat(schema, indent=4, sort_dicts=False)}\n'
                                  f'\n'
                                  f'Instead you should use predefined schema, like: Int(), Str(), Map(), etc.\n'
                                  f'You can see examples in following files:\n'
                                  f'  schemas/tests/test_fundamental_schemas.py\n'
                                  f'  schemas/tests/test_custom_schemas.py\n'
                                  f'\n'
                                  f'You can check all available types here:\n'
                                  f'  schemas/package/schemas/predefined.py')


class Any_(Schema):
    def _create_core_of_schema(self) -> Dict[str, Any]:
        return {}


class Bool(Schema):
    def _create_core_of_schema(self) -> Dict[str, Any]:
        return {'type': 'boolean'}


class Float(Schema):
    def _create_core_of_schema(self) -> Dict[str, Any]:
        return {'type': 'number'}


class Null(Schema):
    def _create_core_of_schema(self) -> Dict[str, Any]:
        return {'type': 'null'}


class Str(Schema):
    def _create_core_of_schema(self) -> Dict[str, Any]:
        return {'type': 'string'}
