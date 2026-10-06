from __future__ import annotations

from typing import TYPE_CHECKING

from api_client_generator.json_rpc import generate_api_description

from api_generation.common import available_apis
from api_generation.model_aliases import apply_stable_model_aliases

if TYPE_CHECKING:
    from pathlib import Path

    from api_generation.common import AvailableApis
    from api_generation.model_variants import ModelVariant


def generate_description(api: AvailableApis, base_directory: Path, variant: ModelVariant) -> Path:
    """
    Generate models and the endpoint description of the API in the given variant (public or validation).

    Returns:
        Path of the generated description module.
    """
    apis_to_skip = [available_api for available_api in available_apis if available_api != api]

    api_description_dict_name = f"{api}_description"
    api = api.replace("-", "_")

    openapi_json_path = base_directory.parent / "documentation" / "openapi.json"
    openapi_flattened_json_path = base_directory.parent / "documentation" / "openapi_flattened.json"

    print(f"Attempting to process {api} ({variant.name} models) from Swagger file: {openapi_json_path}")

    output_path = variant.description_file(base_directory, api)
    output_path.parent.mkdir(parents=True, exist_ok=True)

    generate_api_description(
        api_description_dict_name,
        openapi_json_path,
        output_path,
        openapi_flattened_json_path,
        apis_to_skip=apis_to_skip,
        allow_passing_item_suffix=False,
        model_options=variant.model_options,
        custom_types=variant.custom_types,
    )
    apply_stable_model_aliases(
        output_path,
        api,
        common_file=variant.package_directory(base_directory) / "common.py",
        common_import=variant.common_import,
    )

    return output_path
