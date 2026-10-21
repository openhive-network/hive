# ruff: noqa: INP001
from __future__ import annotations

import ast
import importlib.util
import sys
from pathlib import Path
from typing import TYPE_CHECKING, cast

from api_client_generator.json_rpc import append_endpoint_results
from api_client_generator.tidy import tidy_with_ruff
from api_generation.generate_client import generate_client
from api_generation.generate_description import generate_descriptions
from api_generation.model_methods import add_model_methods
from api_generation.model_variants import PUBLIC, VALIDATION, VARIANTS
from api_generation.undeclared_dates import check_no_undeclared_dates
from jinja2 import Environment, FileSystemLoader

if TYPE_CHECKING:
    from api_client_generator._private.common.models_aliased import ApiDescription


def exported_names(file_path: Path) -> list[str]:
    """Names exported by a generated module - its `__all__` (see `api_client_generator.json_rpc.shared_models`)."""
    for node in ast.parse(file_path.read_text(), filename=str(file_path)).body:
        if not isinstance(node, ast.Assign):
            continue
        if any(isinstance(target, ast.Name) and target.id == "__all__" for target in node.targets):
            names = ast.literal_eval(node.value)
            assert isinstance(names, list), f"`__all__` of {file_path} is a list"
            return [str(name) for name in names]
    raise ValueError(f"{file_path} has no `__all__`")


def load_symbol_from_file(file_path: Path, symbol_name: str) -> object:
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
    project_directory = file_path.parents[2]
    if str(project_directory) not in sys.path:
        sys.path.insert(0, str(project_directory))

    relative_path = file_path.relative_to(project_directory)
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
        (api_subpackage_path / output_name).write_text(content)


def render_validation_package(base_directory: Path, template_directory: Path) -> None:
    """Render `hiveio_api/_validation/__init__.py` - the package only holds lazily imported modules."""
    env = Environment(loader=FileSystemLoader(template_directory))
    package_directory = VALIDATION.package_directory(base_directory)
    package_directory.mkdir(parents=True, exist_ok=True)
    (package_directory / "__init__.py").write_text(env.get_template("_validation/__init__.py.j2").render())


def render_common_package(
    shared: dict[str, str], common_import: str, common_package: Path, template_directory: Path
) -> None:
    """Render `__init__.py` of the package of models shared by several APIs - importing the models on first access."""
    env = Environment(loader=FileSystemLoader(template_directory))
    models = [{"name": name, "module": f"{common_import}.{shared[name]}"} for name in sorted(shared)]
    (common_package / "__init__.py").write_text(env.get_template("common/__init__.py.j2").render(models=models))


def generate_models(apis: list[str], base_directory: Path) -> None:
    """Generate models of the APIs in both variants - the first stage, followed by clients of the APIs."""
    template_directory = base_directory / "python_api_package" / "templates"
    openapi_json_path = base_directory.parent / "documentation" / "openapi.json"
    for api in apis:
        create_api_directory_structure(api, base_directory, template_directory / "api")
        check_no_undeclared_dates(openapi_json_path, api)

    render_validation_package(base_directory, template_directory)
    for variant in VARIANTS:
        shared = generate_descriptions(apis, base_directory, variant)
        common_package = variant.common_package(base_directory)
        if shared:
            render_common_package(shared, variant.common_import, common_package, template_directory)
        description_files = [variant.description_file(base_directory, api) for api in apis]
        if variant is PUBLIC:
            add_model_methods([*description_files, *(common_package / f"{module}.py" for module in shared.values())])
        if variant is VALIDATION:
            for api, validation_file in zip(apis, description_files, strict=True):
                append_endpoint_results(validation_file, f"{api}_description")
        tidy_with_ruff(*description_files, common_package)
    tidy_with_ruff(VALIDATION.package_directory(base_directory) / "__init__.py")


def generate_package(api: str, base_directory: Path) -> None:
    """Generate the client and the subpackage of the API from its final description - after models of all APIs."""
    template_api_path = base_directory / "python_api_package" / "templates" / "api"
    api_description_file = PUBLIC.description_file(base_directory, api.replace("-", "_"))
    description_symbol_name = f"{api.replace('-', '_')}_description"

    print(f"Loading generated API descriptor: {description_symbol_name} from generated file: {api_description_file}")

    # the description written by the generator itself (`generate_api_description`)
    api_descriptor = cast("ApiDescription", load_symbol_from_file(api_description_file, description_symbol_name))
    generate_client(api, api_descriptor, base_directory)

    api_name_snake_case = api.replace("-", "_")
    api_name_pascal_case = "".join(word.capitalize() for word in api_name_snake_case.split("_"))
    api_subpackage_path = base_directory / "python_api_package" / "hiveio_api" / api_name_snake_case

    # names of the description, imported lazily by the subpackage
    description_symbols = exported_names(api_description_file)
    print(f"Extracted {len(description_symbols)} symbols from description file")

    # Render subpackage template files (__init__.py, __main__.py, example.py, README.md)
    render_package_templates(
        api_name_snake_case,
        api_name_pascal_case,
        description_symbols,
        api_subpackage_path,
        template_api_path,
    )

    tidy_with_ruff(api_subpackage_path)
    print(f"Successfully generated API subpackage: hiveio_api.{api_name_snake_case}")


if __name__ == "__main__":
    if len(sys.argv) < 3:
        raise ValueError("Usage: python generate_api_definitions.py <base_directory> <api_name> [<api_name> ...]")

    base_directory = Path(sys.argv[1])
    apis = sys.argv[2:]
    generate_models(apis, base_directory)
    for api in apis:
        generate_package(api, base_directory)
