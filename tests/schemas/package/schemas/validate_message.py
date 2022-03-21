from typing import Any, Dict

from pykwalify.core import Core
import pykwalify.errors


def validate_message(message: Any, schema: Dict) -> None:
    core = Core(
        source_data=message,
        schema_data=schema,
    )

    try:
        core.validate(raise_exception=True)
    except pykwalify.errors.SchemaError as e:
        raise AssertionError(e.msg)
    except pykwalify.errors.RuleError as e:
        raise AssertionError(e.msg)
