from typing import Any, Dict

from pykwalify.core import Core
import pykwalify.errors

from schemas.__private import pykwalify_extensions


def validate_message(message: Any, schema: Dict) -> None:
    core = Core(
        source_data=message,
        schema_data=schema,
        extensions=[pykwalify_extensions.__file__]
    )

    try:
        core.validate(raise_exception=True)
    except pykwalify.errors.SchemaError as e:
        raise AssertionError(e.msg)
    except pykwalify.errors.RuleError as e:
        raise AssertionError(e.msg)
