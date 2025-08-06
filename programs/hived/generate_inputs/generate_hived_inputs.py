from __future__ import annotations
import re
import subprocess
from sys import argv
from pathlib import Path
from typing import Final
from api_client_generator.generate_types_from_swagger import generate_types_from_swagger


PARENT_PATH: Final[Path] = Path(__file__).absolute().parent
EXPECTED_AMOUNT_OF_ARGUMENTS: Final[int] = 5
PYPROJECT_FILE = "pyproject.toml"
PYPROJECT_IN_FILE = "pyproject.toml.in"
BEEKEEPY_VERSION_REPLACE_STRING = "###BEEKEEPY VERSION####"
HELP_TEXT: Final[str] = f"""
Usage: python {__file__} <name> <input_swagger_directory> <output_directory> <path_to_api_generation_pyproject>

name: The name of the binary for which to generate the schema.
input_swagger_directory: The directory containing the openapi-arguments.json and openapi-config.json files.
output_directory: The directory where the generated schema files will be saved.
path_to_api_generation_pyproject: The path to the pyproject.toml file.
"""


def generate_description(
    input_file: Path, output_directory: Path, name: str, schema_type: str
) -> Path:
    print(
        f"Attempting to process {schema_type} of {name} from Swagger file: {input_file}"
    )

    package_path = output_directory / f"{name}_inputs"
    package_path.mkdir(exist_ok=True)
    output_path = package_path / f"{name}_{schema_type}.py"

    generate_types_from_swagger(
        openapi_api_definition=input_file,
        output=output_path,
        base_class={
            "config": "beekeepy.handle.runnable.Config",
            "arguments": "beekeepy.handle.runnable.Arguments",
        }[schema_type],
        custom_class_name_generator=lambda _: f"{name.capitalize()}{schema_type.capitalize()}",
        apply_default_values_for_required_fields=True,
        use_union_operator=True,
        strict_nullable=True,
    )

    return output_path


def get_verison_of_beekeepy_from_pyproject_toml_file(path_to_pyproject: Path) -> str:
    """
    Reads the pyproject.toml file and returns the version of beekeepy.
    """
    beekeepy_version_regex = re.compile(r"""^beekeepy\s*=\s*"([^"]+)" """, re.MULTILINE)

    with path_to_pyproject.open() as f:
        for line in f:
            if beekeepy_version_regex.match(line):
                return line.strip()
    raise ValueError(f"beekeepy version not found in {path_to_pyproject}")


def generate_arguments_and_config_files(
    name: str, input_swagger_directory: Path, output_directory: Path
) -> None:
    """
    Generate arguments and config files from swagger definitions.
    """
    for schema_type in ["arguments", "config"]:
        swagger_file_path = input_swagger_directory / f"openapi-{schema_type}.json"
        if not swagger_file_path.exists():
            print(f"Swagger file {swagger_file_path} does not exist.")
            return
        generate_description(
            input_file=swagger_file_path,
            output_directory=output_directory,
            name=name,
            schema_type=schema_type,
        )


def generate_pyproject_toml_file(
    output_directory: Path, path_to_api_generation_pyproject: Path
) -> None:
    """
    Generate pyproject.toml file by replacing beekeepy version placeholder.
    """
    beekeepy_version = get_verison_of_beekeepy_from_pyproject_toml_file(
        path_to_api_generation_pyproject
    )
    processed_pyproject = (
        (output_directory / PYPROJECT_IN_FILE)
        .read_text()
        .replace(BEEKEEPY_VERSION_REPLACE_STRING, beekeepy_version)
    )
    (output_directory / PYPROJECT_FILE).write_text(processed_pyproject)


def parse_command_line_arguments() -> tuple[str, Path, Path, Path]:
    """
    Parse and validate command line arguments.
    Returns a tuple of (name, input_swagger_directory, output_directory, path_to_api_generation_pyproject).
    """
    if len(argv) != EXPECTED_AMOUNT_OF_ARGUMENTS:
        print(
            f"Invalid number of arguments, expected {EXPECTED_AMOUNT_OF_ARGUMENTS}, got: {len(argv)}."
        )
        print(HELP_TEXT)
        exit(1)

    # Read command line arguments
    name = str(argv[1])
    input_swagger_directory = Path(argv[2]).resolve().absolute()
    output_directory = Path(argv[3]).resolve().absolute()
    path_to_api_generation_pyproject = Path(argv[4]).resolve().absolute()

    return (
        name,
        input_swagger_directory,
        output_directory,
        path_to_api_generation_pyproject,
    )


def main() -> None:
    (
        name,
        input_swagger_directory,
        output_directory,
        path_to_api_generation_pyproject,
    ) = parse_command_line_arguments()

    # Generate arguments and config files
    generate_arguments_and_config_files(name, input_swagger_directory, output_directory)

    # Generate pyproject.toml file
    generate_pyproject_toml_file(output_directory, path_to_api_generation_pyproject)

    # Lock poetry dependencies
    poetry_lock_command = [
        "poetry",
        "-C",
        output_directory.absolute().as_posix(),
        "lock",
    ]
    subprocess.run(poetry_lock_command, check=True)


if __name__ == "__main__":
    main()
