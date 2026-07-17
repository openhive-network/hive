# ruff: noqa: INP001
"""Generate root package files for hiveio_api."""

from __future__ import annotations

import sys
from pathlib import Path

from jinja2 import Environment, FileSystemLoader


def generate_root_package(
    api_list: list[str],
    base_directory: Path,
    template_directory: Path,
) -> None:
    """
    Generate the root hiveio_api package files (__init__.py and README.md).

    These files contain the dynamic API list and are rendered from Jinja2 templates.
    Infrastructure files (pyproject.toml, poetry.lock, .gitignore) are committed
    directly in the repository.

    Args:
        api_list: List of API names (snake_case).
        base_directory: The base directory containing the api_generation folder.
        template_directory: The directory containing the root template files.
    """
    package_directory = base_directory / "python_api_package" / "hiveio_api"
    package_directory.mkdir(parents=True, exist_ok=True)

    apis = []
    for api_name in api_list:
        snake_case = api_name.replace("-", "_")
        pascal_case = "".join(word.capitalize() for word in snake_case.split("_"))
        apis.append({
            "snake_case": snake_case,
            "pascal_case": pascal_case,
        })

    env = Environment(loader=FileSystemLoader(template_directory))

    for template_name, output_name in [
        ("__init__.py.j2", "__init__.py"),
        ("_optional.py.j2", "_optional.py"),
        ("README.md.j2", "README.md"),
    ]:
        template = env.get_template(template_name)
        content = template.render(apis=apis)
        (package_directory / output_name).write_text(content)

    print(f"Generated root package files with {len(apis)} APIs: {[a['snake_case'] for a in apis]}")


if __name__ == "__main__":
    if len(sys.argv) < 3:
        raise ValueError("Usage: python generate_root_package.py <base_directory> <api_name1> [api_name2] ...")

    base_directory = Path(sys.argv[1])
    api_list = sys.argv[2:]

    template_api_path = base_directory / "python_api_package" / "templates"
    generate_root_package(api_list, base_directory, template_api_path)
