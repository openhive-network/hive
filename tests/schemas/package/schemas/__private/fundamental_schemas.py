from typing import Any, Dict


def __create_basic_schema(type_: str, required: bool, options: Dict) -> Dict[str, Any]:
    return {
        'type': type_,
        'required': required,
        **options,
    }


def Str(required: bool = True, **options) -> Dict[str, Any]:
    return __create_basic_schema('str', required, options)
