"""
Location of the generated hiveio_api package.

Tests importing the package (also its hand-written modules - the package `__init__.py` is generated) are skipped
when it is not generated (generate_api_packages.sh) - unless `HIVEIO_API_REQUIRE_GENERATED_PACKAGE` is set (CI),
then they fail.
"""

from __future__ import annotations

import os
import sys
from pathlib import Path
from typing import TYPE_CHECKING, Final

import pytest

if TYPE_CHECKING:
    from collections.abc import Iterable

PACKAGE_ROOT: Final[Path] = Path(__file__).parents[2] / "python_api_package"
PACKAGE_DIR: Final[Path] = PACKAGE_ROOT / "hiveio_api"
TEMPLATE_DIR: Final[Path] = PACKAGE_ROOT / "hiveio_api_template"
"""Hand-written modules of the package - copied into it before the generated ones."""
OPENAPI: Final[Path] = Path(__file__).parents[3] / "documentation" / "openapi.json"


def generated_modules(paths: Iterable[Path]) -> list[Path]:
    """Modules of the package among the given ones which are generated - not copied from the template."""
    return [path for path in paths if not (TEMPLATE_DIR / path.relative_to(PACKAGE_DIR)).exists()]


def require_generated_package() -> None:
    """Skip the calling test module when the package is not generated, make the package importable otherwise."""
    generated = (PACKAGE_DIR / "__init__.py").exists()
    if not generated and not os.environ.get("HIVEIO_API_REQUIRE_GENERATED_PACKAGE"):
        pytest.skip("hiveio_api package is not generated", allow_module_level=True)
    if str(PACKAGE_ROOT) not in sys.path:
        sys.path.insert(0, str(PACKAGE_ROOT))
