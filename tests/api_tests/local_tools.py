import json
from pathlib import Path
from typing import Any, Dict


def read_from_json_pattern(directory: Path, method_name: str) -> Dict[str, Any]:
    with open(f'{directory}/{method_name}.pat.json', 'r') as json_file:
        return json.load(json_file)


def write_to_json_pattern(directory: Path, method_name: str, json_response) -> None:
    directory.mkdir(parents=True, exist_ok=True)
    with open(f'{directory}/{method_name}.pat.json', 'w') as json_file:
        json.dump(json_response, json_file)
