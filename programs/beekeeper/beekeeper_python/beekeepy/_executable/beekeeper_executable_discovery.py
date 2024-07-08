from __future__ import annotations

from pathlib import Path
from typing import Final

BEEKEEPER_BINARY_NAME: Final[str] = "beekeeper"


def get_beekeeper_binary_path() -> Path:
    me = Path(__file__).absolute()
    _process_dir = me.parent
    main_package_dir = _process_dir.parent
    glob_results = list(main_package_dir.glob(BEEKEEPER_BINARY_NAME))
    found_beekeeper_executable = len(glob_results)
    assert (
        found_beekeeper_executable == 1
    ), f"There should be exacly one executable, but found: {found_beekeeper_executable}."
    beekeeper_file_path = glob_results[0]
    assert beekeeper_file_path.is_file(), "Found path of beekeeper binary is not a file"
    return beekeeper_file_path
