from __future__ import annotations

from pathlib import Path

BEEKEEPER_BINARY_NAME = "beekeeper"


def get_beekeeper_binary_path() -> Path:
    me = Path(__file__).absolute()
    _process_dir = me.parent
    main_package_dir = _process_dir.parent
    glob_results = list(main_package_dir.glob(BEEKEEPER_BINARY_NAME))
    assert len(glob_results) == 1
    beekeeper_file = glob_results[0]
    assert beekeeper_file.is_file()
    return beekeeper_file
