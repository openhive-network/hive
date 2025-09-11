from __future__ import annotations

from pathlib import Path
from typing import Any, TYPE_CHECKING

from beekeepy.handle.remote import AbstractAsyncApi
from api_client_generator.json_rpc import generate_api_client

from api_generation.common import APIS_WITH_LEGACY_ARGS_SERIALIZATION

if TYPE_CHECKING:
    from api_generation.common import AvailableApis



def generate_client(api: AvailableApis, api_description: dict[str, Any], base_directory: Path) -> None:

    api = api.replace("-", "_")
    generated_client_output_path = base_directory / api / api / f"{api}_client.py"

    generate_api_client(
        api_description,
        AbstractAsyncApi,
        path=generated_client_output_path,
        legacy_args_serialization=api in APIS_WITH_LEGACY_ARGS_SERIALIZATION,
    )
