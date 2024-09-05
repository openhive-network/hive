from __future__ import annotations

from pathlib import Path
from typing import Final

BEEKEEPER_BINARY_NAME: Final[str] = "beekeeper"


def get_root_package_directory() -> Path:
    me = Path(__file__).absolute()
    _process_dir = me.parent
    return _process_dir.parent


def get_beekeeper_binary_path() -> Path:
    root_package_dir = get_root_package_directory()
    glob_results = list(root_package_dir.glob(BEEKEEPER_BINARY_NAME))
    found_beekeeper_executable = len(glob_results)
    assert (
        found_beekeeper_executable == 1
    ), f"There should be exacly one executable, but found: {found_beekeeper_executable}."
    beekeeper_file_path = glob_results[0]
    assert beekeeper_file_path.is_file(), "Found path of beekeeper binary is not a file"
    return beekeeper_file_path
