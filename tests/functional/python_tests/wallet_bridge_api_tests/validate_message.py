from pykwalify.core import Core
import partial_schemas


def validate_message(message, schema):
    return Core(source_data=message, schema_data=schema, extensions=[partial_schemas.__file__]).validate(raise_exception=True)
