from __future__ import annotations

from pathlib import Path
from typing import TYPE_CHECKING

from api_generation.common import available_apis
from api_client_generator.json_rpc import generate_api_description

if TYPE_CHECKING:
    from api_generation.common import AvailableApis


def generate_description(api: AvailableApis, base_directory: Path) -> Path:
    apis_to_skip = available_apis
    apis_to_skip.remove(api)

    api_description_dict_name = f"{api}_description"

    api_generation_path = base_directory 
    openapi_json_path = base_directory.parent / "documentation" / "openapi.json"
    openapi_flattened_json_path = base_directory.parent / "documentation" / "openapi_flattened.json"

    print(f"Attempting to process {api} from Swagger file: {openapi_json_path}")

    output_path = api_generation_path / api / api / f"{api}_description.py"

    generate_api_description(
        api_description_dict_name,
        openapi_json_path,
        output_path,
        openapi_flattened_json_path,
        apis_to_skip=apis_to_skip,
    )

    return output_path
