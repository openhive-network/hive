import pykwalify.errors
from pykwalify.core import Core
from schemas.package.schemas import fundamental_schemas


def validate_message(message, schema):
    core = Core(
        source_data=message,
        schema_data=schema,
        extensions=[fundamental_schemas.__file__]
    )

    try:
        core.validate(raise_exception=True)
    except pykwalify.errors.SchemaError as e:
        raise AssertionError
    # except pykwalify.errors.CoreError as e:  #??
    #     raise AssertionError
    except pykwalify.errors.RuleError as e:
        raise AssertionError
