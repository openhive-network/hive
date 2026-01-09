"""Generate root package files for hiveio_api."""

from __future__ import annotations

import shutil
import sys
from pathlib import Path

import toml
from jinja2 import Environment, FileSystemLoader


def generate_root_package(
    api_list: list[str],
    base_directory: Path,
    template_directory: Path,
) -> None:
    """
    Generate the root hiveio_api package files.

    Structure created:
        base_directory/hiveio_api/          <- PYPROJECT_DIR
        ├── pyproject.toml
        └── hiveio_api/                     <- Python package
            ├── __init__.py
            ├── README.md
            ├── py.typed
            └── {api_name}/                 <- API subpackages

    Args:
        api_list: List of API names (snake_case).
        base_directory: The base directory containing the api_generation folder.
        template_directory: The directory containing the root template files.
    """
    # hiveio_api/ is the poetry project directory
    project_directory = base_directory / "hiveio_api"
    # hiveio_api/hiveio_api/ is the actual Python package
    package_directory = project_directory / "hiveio_api"

    project_directory.mkdir(exist_ok=True)
    package_directory.mkdir(exist_ok=True)

    # Prepare API data for templates
    apis = []
    for api_name in api_list:
        snake_case = api_name.replace("-", "_")
        pascal_case = "".join(word.capitalize() for word in snake_case.split("_"))
        apis.append({
            "snake_case": snake_case,
            "pascal_case": pascal_case,
        })

    # Load api_generation pyproject.toml for beekeepy version
    api_generation_pyproject_path = base_directory / "api_generation" / "pyproject.toml"
    api_generation_pyproject = toml.load(api_generation_pyproject_path)

    # Setup Jinja2 environment
    root_template_dir = template_directory / "root"
    env = Environment(loader=FileSystemLoader(root_template_dir))

    # Render __init__.py (in the Python package directory)
    init_template = env.get_template("__init__.py.j2")
    init_content = init_template.render(apis=apis)
    with open(package_directory / "__init__.py", "w") as f:
        f.write(init_content)

    # Render README.md (in the Python package directory)
    readme_template = env.get_template("README.md.j2")
    readme_content = readme_template.render(apis=apis)
    with open(package_directory / "README.md", "w") as f:
        f.write(readme_content)

    # Render pyproject.toml (in the project directory, parent of Python package)
    pyproject_template = env.get_template("pyproject.toml.j2")
    pyproject_content = pyproject_template.render(
        apis=apis,
        api_generation_pyproject=api_generation_pyproject,
    )
    with open(project_directory / "pyproject.toml", "w") as f:
        f.write(pyproject_content)

    # Render .gitignore (in the project directory)
    gitignore_template = env.get_template(".gitignore.j2")
    gitignore_content = gitignore_template.render()
    with open(project_directory / ".gitignore", "w") as f:
        f.write(gitignore_content)

    # Copy py.typed marker to the Python package directory
    shutil.copy(template_directory / "py.typed", package_directory)

    print(f"Generated root package with {len(apis)} APIs: {[a['snake_case'] for a in apis]}")


if __name__ == "__main__":
    if len(sys.argv) < 3:
        raise ValueError(
            "Usage: python generate_root_package.py <base_directory> <api_name1> [api_name2] ..."
        )

    base_directory = Path(sys.argv[1])
    api_list = sys.argv[2:]

    template_api_path = base_directory / "template_api"
    generate_root_package(api_list, base_directory, template_api_path)
