from __future__ import annotations

import json
from typing import TYPE_CHECKING, Any

if TYPE_CHECKING:
    from pathlib import Path


def read_from_json_pattern(directory: Path, method_name: str) -> dict[str, Any]:
    with open(directory / f"{method_name}.pat.json") as json_file:
        return json.load(json_file)


def write_to_json_pattern(directory: Path, method_name: str, json_response) -> None:
    directory.mkdir(parents=True, exist_ok=True)
    with open(directory / f"{method_name}.pat.json", "w") as json_file:
        json.dump(json_response, json_file)
