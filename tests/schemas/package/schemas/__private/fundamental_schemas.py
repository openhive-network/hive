from typing import Any, Dict, Optional

from schemas.__private.pykwalify_extensions import sequence_strictly_matches_rule


def __create_basic_schema(type_: str, required: bool, options: Dict) -> Dict[str, Any]:
    return {
        'type': type_,
        'required': required,
        **options,
    }


def Any_(required: bool = True, **options) -> Dict[str, Any]:
    return __create_basic_schema('any', required, options)


def Bool(required: bool = True, **options) -> Dict[str, Any]:
    return __create_basic_schema('bool', required, options)


def Date(required: bool = True) -> Dict[str, Any]:
    return __create_basic_schema('date', required, options={'format': '%Y-%m-%dT%H:%M:%S'})


def Float(required: bool = True, **options) -> Dict[str, Any]:
    return __create_basic_schema('float', required, options)


def Int(required: bool = True, **options):
    return __create_basic_schema('hive_int', required, options)


def Map(content: Dict, required: bool = True, matching: str = 'all') -> Dict[str, Any]:
    return {
        'required': required,
        'matching': matching,
        'map': content,
    }


def None_(required: bool = True, **options) -> Dict[str, Any]:
    return __create_basic_schema('none', required, options)


def Seq(*content, required: bool = True, matching: str = 'any',
        options: Optional[Dict[str, Any]] = None) -> Dict[str, Any]:
    if options is None:
        options = {}

    common = {
        'required': required,
        'seq': [*content],
        **options,
    }

    if matching == 'strict':
        return {
            'func': sequence_strictly_matches_rule.__name__,
            **common,
        }

    return {
        'matching': matching,
        **common,
    }


def Str(required: bool = True, **options) -> Dict[str, Any]:
    return __create_basic_schema('str', required, options)
