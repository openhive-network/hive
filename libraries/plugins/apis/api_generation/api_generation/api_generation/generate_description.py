from __future__ import annotations

from pathlib import Path
from typing import TYPE_CHECKING

from api_generation.common import available_apis
from schemas.apis.api_client_generator import generate_json_rpc_api_description

if TYPE_CHECKING:
    from api_generation.common import AvailableApis


def generate_description(api: AvailableApis, base_directory: Path) -> None:
    apis_to_skip = available_apis.copy()
    apis_to_skip.remove(api)

    api_description_dict_name = f"{api}_description"

    api_generation_path = base_directory 
    openapi_json_path = base_directory.parent / "documentation" / "openapi.json"

    print(f"Attempting to process {api} from Swagger file: {openapi_json_path}")

    generate_json_rpc_api_description(
        api_description_dict_name,
        openapi_json_path,
        api_generation_path / api / api / f"{api}_description.py",
        apis_to_skip=apis_to_skip,
    )
