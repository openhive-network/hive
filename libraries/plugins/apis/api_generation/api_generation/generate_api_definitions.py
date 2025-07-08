from __future__ import annotations

from typing import Any, TYPE_CHECKING

from api_generation.generate_description import generate_description
from api_generation.generate_client import generate_client

import importlib.util
from pathlib import Path
import sys
import toml

def load_symbol_from_file(file_path: Path, symbol_name: str) -> dict[str, Any]:
    module_name = file_path.stem  # e.g., 'my_module' from 'my_module.py'
    spec = importlib.util.spec_from_file_location(module_name, str(file_path))
    module = importlib.util.module_from_spec(spec)
    sys.modules[module_name] = module
    spec.loader.exec_module(module)
    return getattr(module, symbol_name)

def get_dependency_versions(pyproject_path: Path, deps: list[str]) -> dict[str, str]:
    """Extract specified dependency versions from a pyproject.toml file."""
    data = toml.load(pyproject_path)
    poetry_deps = data["tool"]["poetry"]["dependencies"]
    result = {}
    for dep in deps:
        if dep in poetry_deps:
            result[dep] = poetry_deps[dep]
    return result

if __name__ == "__main__":
    if len(sys.argv) == 3:
        api_name = sys.argv[1]
        base_directory = Path(sys.argv[2])
    else:
        raise ValueError("Usage: python generate_api_definitions.py <api_name> <base_directory>")
    api_description_file = generate_description(api_name, base_directory)

    # Generated file contains a symbol pointing to array descriptor table to be usually imported as e.g.:
    # from account_by_key_api.account_by_key_api_description import account_by_key_api_description

    description_symbol_name = f"{api_name}_description"

    print(f"Loading generated API descriptor: {description_symbol_name} from generated file: {api_description_file}")
    api_descriptor = load_symbol_from_file(api_description_file, description_symbol_name)

    #print(f"Generated API description for {api_name}: {api_descriptor}")

    generate_client(api_name, api_descriptor, base_directory)

    deps_to_copy = ["schemas", "beekeepy"]

    pyproject_path = base_directory / "api_generation" / "pyproject.toml"

    actual_deps = get_dependency_versions(pyproject_path, deps_to_copy)

    print(f"Actual dependencies for {api_name} package: {actual_deps}")

    api_pyproject_path = base_directory / f"{api_name}" / "pyproject.toml"
    pyproject = toml.load(api_pyproject_path)

    source = [
        { "name": "PyPI", "priority": "primary" },
        { "name": "gitlab-schemas", "url": "https://gitlab.syncad.com/api/v4/projects/362/packages/pypi/simple", "priority": "supplemental" },
        { "name": "gitlab-beekeepy", "url": "https://gitlab.syncad.com/api/v4/projects/434/packages/pypi/simple", "priority": "supplemental" },
    ]

    pyproject["tool"]["poetry"]["source"] = source

    # Inject or update dependencies
    pyproject["tool"]["poetry"]["dependencies"].pop("api_generation", None)
    pyproject["tool"]["poetry"]["dependencies"]["schemas"] = actual_deps["schemas"]
    pyproject["tool"]["poetry"]["dependencies"]["beekeepy"] = actual_deps["beekeepy"]

    with open(api_pyproject_path, "w") as f:
        toml.dump(pyproject, f)
