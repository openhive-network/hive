from __future__ import annotations

import os
from distutils.util import strtobool
from typing import TYPE_CHECKING

from hive_local_tools.api import read_from_json_pattern, write_to_json_pattern

if TYPE_CHECKING:
    from pathlib import Path


def __read_from_text_pattern(directory: Path, method_name: str) -> str:
    with open(f"{directory}/{method_name}.pat.txt") as text_file:  # noqa FURB101
        return text_file.read()


def __write_to_text_pattern(directory: Path, method_name: str, text: str) -> None:
    directory.mkdir(parents=True, exist_ok=True)
    with open(f"{directory}/{method_name}.pat.txt", "w") as text_file:  # noqa FURB103
        text_file.write(text)


def verify_json_patterns(directory: Path, method_name, method_output):
    """
    Asserts that `method_output` is same as corresponding pattern. Optionally updates (overrides)
    existing patterns with actual method output, if `GENERATE_PATTERNS` environment variable is set.
    """
    generate_patterns = strtobool(os.environ.get("GENERATE_PATTERNS", "OFF"))
    if not generate_patterns:
        pattern = read_from_json_pattern(directory, method_name)
        assert pattern == method_output, f"Wrong method output: {method_output}"

    write_to_json_pattern(directory, method_name, method_output)


def verify_text_patterns(directory: Path, method_name, method_output):
    """
    Asserts that `method_output` is same as corresponding pattern. Optionally updates (overrides)
    existing patterns with actual method output, if `GENERATE_PATTERNS` environment variable is set.
    """
    generate_patterns = strtobool(os.environ.get("GENERATE_PATTERNS", "OFF"))
    if not generate_patterns:
        pattern = __read_from_text_pattern(directory, method_name)
        assert pattern == method_output, f"Wrong method output: {method_output}"

    __write_to_text_pattern(directory, method_name, method_output)
