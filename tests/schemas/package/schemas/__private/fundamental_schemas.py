from abc import ABC, abstractmethod
from typing import Any, Dict, List, Optional

from schemas.__private import custom_validator


class Schema(ABC):
    def __init__(self, **options: Any):
        self._options = options

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


class Any_(Schema):
    def _create_core_of_schema(self) -> Dict[str, Any]:
        return {}


class Array(Schema):
    def __init__(self, *items, **options: Any):
        super().__init__(**options)
        self.__items = list(items)

    def _create_core_of_schema(self) -> Dict[str, Any]:
        items_as_list = []
        for schema in self.__items:
            if isinstance(schema, Schema):
                items_as_list.append(schema._create_schema())
        if len(items_as_list) > 1:
            return {
                'type': 'array',
                'items': {
                    'oneOf': items_as_list
                },
            }
        elif len(items_as_list) == 0:
            return {
                'type': 'array',
            }
        return {
            'type': 'array',
            'items': items_as_list[0],
        }


class ArrayStrict(Schema):
    def __init__(self, *items, **options: Any):
        """
        Array Strict checks that the nth element in the instance, matches the nth type specified in the schema.
            example_array =  [0, 'string', True]
            example_schema : ArrayStrict(Int(), String(), Bool())
                example_array[0] -> Int(),
                example_array[1] -> String(),
                example_array[2] -> Bool()
        """
        super().__init__(**options)
        self.__items = list(items)

    def _create_core_of_schema(self) -> Dict[str, Any]:
        items_as_list = self.__items.copy()
        for index, schema in enumerate(items_as_list):
            if isinstance(schema, Schema):
                items_as_list[index] = schema._create_schema()

        return {
            'type': 'array',
            'prefixItems': items_as_list,
            "minItems": len(items_as_list),
            'maxItems': len(items_as_list),
        }


class Bool(Schema):
    def _create_core_of_schema(self) -> Dict[str, Any]:
        return {'type': 'boolean'}


class Float(Schema):
    def _create_core_of_schema(self) -> Dict[str, Any]:
        return {'type': 'number'}


class Int(Schema):
    def _create_core_of_schema(self) -> [str, Any]:
        return {
            'anyOf': [
                {'type': 'integer'},
                {'type': 'string', 'pattern': r'^-?\d+$'},
            ]
        }


class Null(Schema):
    def _create_core_of_schema(self) -> Dict[str, Any]:
        return {'type': 'null'}


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
        schema_as_dicts = self.__schema.copy()
        for key, schema in self.__schema.items():
            if isinstance(schema, Schema):
                schema_as_dicts[key] = schema._create_schema()
        return {
            'type': 'object',
            'properties': schema_as_dicts,
            'required': self.__required_keys,
            'additionalProperties': self.__allow_additional_properties
        }


class String(Schema):
    def _create_core_of_schema(self) -> Dict[str, Any]:
        return {'type': 'string'}
