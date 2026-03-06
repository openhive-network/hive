from __future__ import annotations

from pathlib import Path
from typing import Any, TYPE_CHECKING

from beekeepy.handle.remote import AbstractAsyncApi
from api_client_generator.json_rpc import generate_api_client

from api_generation.common import APIS_WITH_LEGACY_ARGS_SERIALIZATION

if TYPE_CHECKING:
    from api_generation.common import AvailableApis


class _ImportableItem:
    """Simple class that matches the Importable Protocol interface."""
    def __init__(self, name: str, source: str) -> None:
        self.__name__ = name
        self.__module__ = source


def generate_client(api: AvailableApis, api_description: dict[str, Any], base_directory: Path) -> None:

    api = api.replace("-", "_")
    generated_client_output_path = base_directory / "python_api_package" / "hiveio_api" / "apis" / api / f"{api}_client.py"

    # Add UNSET import to fix NameError in generated code
    # Use msgspec.UNSET instead of datamodel_code_generator's version to avoid importing
    # datamodel_code_generator at runtime (it's a dev-only dependency)
    additional_imports = [
        _ImportableItem(name="UNSET", source="msgspec"),
    ]

    generate_api_client(
        api_description,
        AbstractAsyncApi,
        path=generated_client_output_path,
        legacy_args_serialization=api in APIS_WITH_LEGACY_ARGS_SERIALIZATION,
        additional_items_to_import=additional_imports,
    )
