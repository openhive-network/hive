from abc import ABC, abstractmethod
import pprint
from typing import Any, Dict, List, Optional

from jsonschema.exceptions import ValidationError

from schemas.__private import custom_validator


class Schema(ABC):
    def __init__(self, **options: Any):
        self._options = options

    def __repr__(self) -> str:
        return str(self._create_schema())

    def validate(self, instance) -> None:
        custom_validator.validate(
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


class Map(Schema):
    def __init__(self, schema: Dict, required_keys: Optional[List[str]] = None,
                 allow_additional_properties: bool = False, **options: Any):
        """
        :param schema: Collection of properties. Each of the individual properties has a key
        (represents the name of the key in instance), and schema (represents the required type,
        for the previously specified key).
        :param required_keys: The keys contained in this parameter are required for successful validation.
        The other keys specified in the schema are optional, their absence will not affect the validation process.
        :param allow_additional_properties: By default map does not accept additional undefined properties.
        All instance elements not defined in the schema will be reported as a 'ValidationError'.
        Set this parameter to True, it will allow the correct validation of additional properties in the instance.
        :param options: Other options that can be given in the form of a dictionary.
        """

        super().__init__(**options)
        self.__allow_additional_properties: bool = allow_additional_properties
        self.__schema = schema
        self.__required_keys: List[str] = list(self.__schema.keys()) if required_keys is None else required_keys

    def _create_core_of_schema(self) -> Dict[str, Any]:
        items_as_dicts = self.__schema.copy()
        for key, schema in self.__schema.items():
            self._assert_that_schema_has_correct_type(schema)
            items_as_dicts[key] = schema._create_schema()
        return {
            'type': 'object',
            'properties': items_as_dicts,
            'required': self.__required_keys,
            'additionalProperties': self.__allow_additional_properties
        }


class Null(Schema):
    def _create_core_of_schema(self) -> Dict[str, Any]:
        return {'type': 'null'}


class Str(Schema):
    def _create_core_of_schema(self) -> Dict[str, Any]:
        return {'type': 'string'}
