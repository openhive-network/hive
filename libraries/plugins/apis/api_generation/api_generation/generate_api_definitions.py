from __future__ import annotations

import shutil
from pathlib import Path
from typing import Any
from jinja2 import Environment, FileSystemLoader
import sys
import toml
import importlib.util

from api_generation.generate_description import generate_description
from api_generation.generate_client import generate_client


def load_symbol_from_file(file_path: Path, symbol_name: str) -> dict[str, Any]:
    """
    Dynamically load a symbol from a Python file.

    Args:
        file_path: The path to the Python file from which to load the symbol.
        symbol_name: The name of the symbol to load from the file.

    Returns:
        The loaded symbol from the specified file.

    Raises:
        FileNotFoundError: If the specified file does not exist.
        ValueError: If the file is not a Python file (does not have a .py extension).
    """
    if not file_path.exists():
        raise FileNotFoundError(f"The file {file_path} does not exist.")

    if file_path.suffix != ".py":
        raise ValueError(f"The file {file_path} is not a Python file.")

    relative_path = file_path.relative_to(file_path.parents[1])
    module_name = ".".join(relative_path.with_suffix("").parts)

    spec = importlib.util.spec_from_file_location(module_name, str(file_path))
    module = importlib.util.module_from_spec(spec)
    sys.modules[module_name] = module
    spec.loader.exec_module(module)

    return getattr(module, symbol_name)


def create_api_directory_structure(
    api_name: str,
    base_directory: Path,
    template_directory: Path,
) -> None:
    """
    Create the directory structure for the API.

    Args:
        api_name: The name of the API.
        base_directory: The base directory where the API structure will be created.
        template_directory: The directory containing the template files for the API.
    """

    api_root_directory = base_directory / api_name
    api_package_path = api_root_directory / api_name

    api_root_directory.mkdir(exist_ok=True)
    api_package_path.mkdir(exist_ok=True)

    shutil.copy(template_directory / "py.typed", api_package_path)
    shutil.copy(template_directory / "__init__.py", api_package_path)


def create_pyproject_content_for_api_package(
    api_name: str,
    shared_versions_path: Path,
    template_directory: Path,
) -> str:
    """
    Create a pyproject.toml content file for the API package.

    Args:
        api_name: The name of the API for which to create the pyproject.toml content.
        shared_versions_path: The path to the shared versions file.
        template_directory: The directory containing the template files for the API.

    Returns:
        The content of the pyproject.toml file as a string.
    """
    versions = toml.load(shared_versions_path)

    env = Environment(loader=FileSystemLoader(template_directory))
    template = env.get_template("pyproject.toml.j2")

    return template.render(api_name=api_name, versions=versions)


if __name__ == "__main__":
    if len(sys.argv) != 3:
        raise ValueError(
            "Usage: python generate_api_definitions.py <api_name> <base_directory>"
        )

    api = sys.argv[1]
    base_directory = Path(sys.argv[2])

    template_api_path = base_directory / "template_api"
    create_api_directory_structure(api, base_directory, template_api_path)

    api_description_file = generate_description(api, base_directory)
    description_symbol_name = f"{api}_description"

    print(
        f"Loading generated API descriptor: {description_symbol_name} from generated file: {api_description_file}"
    )

    api_descriptor = load_symbol_from_file(
        api_description_file, description_symbol_name
    )
    generate_client(api, api_descriptor, base_directory)

    shared_versions = base_directory / "shared_versions.toml"
    pyproject_content = create_pyproject_content_for_api_package(
        api, shared_versions, template_api_path
    )

    api_pyproject_path = base_directory / f"{api}" / "pyproject.toml"

    with open(api_pyproject_path, "w") as f:
        f.write(pyproject_content)
