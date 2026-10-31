from __future__ import annotations

from pathlib import Path
from typing import TYPE_CHECKING, Final

from api_client_generator.json_rpc import generate_api_client

from api_generation.common import APIS_WITH_LEGACY_ARGS_SERIALIZATION
from api_generation.model_variants import VALIDATION

CLIENT_BASE_CLASS: Final[str] = "AbstractAsyncApi"
CLIENT_BASE_CLASS_SOURCE: Final[str] = "hiveio_api._apis.abc.api"

if TYPE_CHECKING:
    from types import ModuleType

    from api_client_generator._private.common.models_aliased import ApiDescription

    from api_generation.common import AvailableApis


def generate_client(
    api: AvailableApis,
    api_description: ApiDescription,
    base_directory: Path,
    description_module: ModuleType | None = None,
) -> None:
    """
    Generate the client of the API - `hiveio_api/<api>/<api>_client.py`.

    Args:
        api: Name of the API.
        api_description: Description of endpoints of the API (of its generated public description module).
        base_directory: The base directory containing the api_generation folder.
        description_module: The module defining `api_description` - searched among imported modules when not given.
    """
    api = api.replace("-", "_")
    generated_client_output_path = base_directory / "python_api_package" / "hiveio_api" / api / f"{api}_client.py"

    generate_api_client(
        api_description,
        CLIENT_BASE_CLASS,
        CLIENT_BASE_CLASS_SOURCE,
        path=generated_client_output_path,
        description_module=description_module,
        legacy_args_serialization=api in APIS_WITH_LEGACY_ARGS_SERIALIZATION,
        class_attributes={
            # location of the models used by optional `validate_schema` - never imported by the client itself
            "__validation_module__": f"{VALIDATION.package}.{api}",
        },
    )
