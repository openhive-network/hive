from __future__ import annotations

import json
import tempfile
from pathlib import Path
from typing import TYPE_CHECKING, Final

from api_client_generator.json_rpc import generate_api_descriptions

from api_generation.undeclared_assets import check_no_undeclared_assets

if TYPE_CHECKING:
    from collections.abc import Sequence

    from api_generation.common import AvailableApis
    from api_generation.model_variants import ModelVariant

DEFAULT_FORM_SUFFIX: Final[str] = "_default"
"""Suffix of components of openapi.json describing the default (HF26) form of a structure (`strip_default_suffixes`)."""


def strip_default_suffixes(openapi: str) -> str:
    """
    Rename components `<name>_default` of openapi.json to `<name>` (and references to them).

    openapi.json names the default (HF26) form of a structure `<name>_default`, next to `<name>_condenser_api` of its
    legacy form - models of the default form keep the plain name (`Account`, `Witness`, ...).

    Args:
        openapi: Text of openapi.json.

    Returns:
        Text of openapi.json with the components renamed.

    Raises:
        ValueError: When `<name>` is a component as well.
    """
    definition = json.loads(openapi)
    schemas: dict[str, object] = definition["components"]["schemas"]
    renamed = {name: name.removesuffix(DEFAULT_FORM_SUFFIX) for name in schemas if name.endswith(DEFAULT_FORM_SUFFIX)}
    if clashing := sorted(name for name in renamed.values() if name in schemas):
        raise ValueError(f"Components {clashing} of openapi.json clash with components `<name>{DEFAULT_FORM_SUFFIX}`")
    definition["components"]["schemas"] = {renamed.get(name, name): schema for name, schema in schemas.items()}
    result = json.dumps(definition)
    for old, new in renamed.items():
        result = result.replace(f'"#/components/schemas/{old}"', f'"#/components/schemas/{new}"')
    return result


def generate_descriptions(apis: Sequence[AvailableApis], base_directory: Path, variant: ModelVariant) -> dict[str, str]:
    """
    Generate models and endpoint descriptions of the APIs in the given variant (public or validation).

    Models of all APIs are generated once - models used by several APIs go to the common package of the variant, a
    module per model (`api_client_generator.json_rpc.shared_models`).

    Args:
        apis: Names of the APIs.
        base_directory: The base directory containing the api_generation folder (openapi.json is in `documentation`
            next to it).
        variant: The model variant to generate.

    Returns:
        Modules of the common package (names in the package) by names of the shared models.
    """
    openapi_json_path = base_directory.parent / "documentation" / "openapi.json"
    print(f"Generating {variant.name} models of {len(apis)} APIs from Swagger file: {openapi_json_path}")

    description_files = {api: variant.description_file(base_directory, api) for api in apis}
    common_package = variant.common_package(base_directory)
    with tempfile.TemporaryDirectory() as directory:
        definition = Path(directory) / openapi_json_path.name
        definition.write_text(strip_default_suffixes(openapi_json_path.read_text()))
        shared = generate_api_descriptions(
            definition,
            description_files,
            common_package,
            variant.common_import,
            allow_passing_item_suffix=False,
            model_options=variant.model_options,
            custom_types=variant.custom_types,
        )
    print(f"{variant.name} models: {len(shared)} models shared by several APIs")

    for api, description_file in description_files.items():
        check_no_undeclared_assets(description_file, api)
    for module in shared.values():
        check_no_undeclared_assets(common_package / f"{module}.py", variant.common_import)
    return shared
