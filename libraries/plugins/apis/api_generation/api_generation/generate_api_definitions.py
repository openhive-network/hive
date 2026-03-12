from __future__ import annotations

import ast
from pathlib import Path
from typing import Any
from jinja2 import Environment, FileSystemLoader
import sys
import importlib.util

from api_generation.generate_description import generate_description
from api_generation.generate_client import generate_client


def extract_class_names_from_file(file_path: Path) -> list[str]:
    """
    Extract all exported symbol names (classes and TypeAlias) from a Python file using AST parsing.

    Args:
        file_path: The path to the Python file from which to extract symbol names.

    Returns:
        A list of symbol names defined in the file.

    Raises:
        FileNotFoundError: If the specified file does not exist.
    """
    if not file_path.exists():
        raise FileNotFoundError(f"The file {file_path} does not exist.")

    with open(file_path, "r") as f:
        tree = ast.parse(f.read(), filename=str(file_path))

    symbols = []
    for node in ast.walk(tree):
        if isinstance(node, ast.ClassDef):
            symbols.append(node.name)
        elif isinstance(node, ast.AnnAssign) and isinstance(node.target, ast.Name):
            symbols.append(node.target.id)
    return symbols


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

    # Create module name relative to python_api_package project directory (parent of hiveio_api package)
    # For path like: .../python_api_package/hiveio_api/rc_api/rc_api_description.py
    # We want module name: hiveio_api.rc_api.rc_api_description
    # parents[2] is the project directory containing hiveio_api/
    relative_path = file_path.relative_to(file_path.parents[2])
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
    Create the directory structure for the API within the hiveio_api package.

    Structure:
        base_directory/python_api_package/hiveio_api/{api_name}/

    Args:
        api_name: The name of the API.
        base_directory: The base directory where the python_api_package will be created.
        template_directory: The directory containing the template files for the API.
    """

    api_name = api_name.replace("-", "_")

    # python_api_package/hiveio_api/{api_name}/
    project_directory = base_directory / "python_api_package"
    package_directory = project_directory / "hiveio_api"
    api_subpackage_path = package_directory / api_name

    project_directory.mkdir(exist_ok=True)
    package_directory.mkdir(exist_ok=True)
    api_subpackage_path.mkdir(exist_ok=True)


def render_package_templates(
    api_snake_case: str,
    api_pascal_case: str,
    description_symbols: list[str],
    api_subpackage_path: Path,
    template_directory: Path,
) -> None:
    """
    Render and write the subpackage template files (__init__.py, __main__.py, example.py).

    Args:
        api_snake_case: The name of the API in snake_case.
        api_pascal_case: The name of the API in PascalCase.
        description_symbols: List of symbols exported from the description module.
        api_subpackage_path: The path to the API subpackage directory within hiveio_api.
        template_directory: The directory containing the template files.
    """
    env = Environment(loader=FileSystemLoader(template_directory))

    template_context = {
        "api_name_snake_case": api_snake_case,
        "api_name_pascal_case": api_pascal_case,
        "description_symbols": description_symbols,
    }

    for template_name, output_name in [
        ("__init__.py.j2", "__init__.py"),
        ("__main__.py.j2", "__main__.py"),
        ("example.py.j2", "example.py"),
        ("README.md.j2", "README.md"),
    ]:
        template = env.get_template(template_name)
        content = template.render(**template_context)
        with open(api_subpackage_path / output_name, "w") as f:
            f.write(content)


if __name__ == "__main__":
    if len(sys.argv) != 3:
        raise ValueError(
            "Usage: python generate_api_definitions.py <api_name> <base_directory>"
        )

    api = sys.argv[1]
    base_directory = Path(sys.argv[2])

    template_api_path = base_directory / "python_api_package" / "templates" / "api"
    create_api_directory_structure(api, base_directory, template_api_path)

    api_description_file = generate_description(api, base_directory)
    description_symbol_name = f"{api.replace('-', '_')}_description"

    print(
        f"Loading generated API descriptor: {description_symbol_name} from generated file: {api_description_file}"
    )

    api_descriptor = load_symbol_from_file(
        api_description_file, description_symbol_name
    )
    generate_client(api, api_descriptor, base_directory)

    api_name_snake_case = api.replace("-", "_")
    api_name_pascal_case = "".join(word.capitalize() for word in api_name_snake_case.split("_"))
    api_subpackage_path = base_directory / "python_api_package" / "hiveio_api" / api_name_snake_case

    # Extract class names from the generated description file for lazy imports
    description_symbols = extract_class_names_from_file(api_description_file)
    print(f"Extracted {len(description_symbols)} symbols from description file")

    # Render subpackage template files (__init__.py, __main__.py, example.py, README.md)
    render_package_templates(
        api_name_snake_case,
        api_name_pascal_case,
        description_symbols,
        api_subpackage_path,
        template_api_path,
    )

    print(f"Successfully generated API subpackage: hiveio_api.{api_name_snake_case}")
