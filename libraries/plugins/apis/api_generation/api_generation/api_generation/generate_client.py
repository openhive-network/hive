from __future__ import annotations

from pathlib import Path
from typing import Any, TYPE_CHECKING

from beekeepy.handle.remote import AbstractAsyncApi
from schemas.apis.api_client_generator import generate_api_client

if TYPE_CHECKING:
    from api_generation.common import AvailableApis



def generate_client(api: AvailableApis, api_description: dict[str, Any], base_directory: Path) -> None:
    generated_client_output_path = base_directory / api /api / f"{api}_client.py"

    generate_api_client(
        api_description,
        AbstractAsyncApi,
        path=generated_client_output_path,
    )
