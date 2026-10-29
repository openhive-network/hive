"""
Generator of the hiveio_api package.

The generator uses the field types of hiveio_api (`hiveio_api.fields`) - imported from the package being generated,
which is not installed: generate_api_packages.sh copies its hand-written modules (hiveio_api_template) before
the generated ones.
"""

from __future__ import annotations

import sys
from pathlib import Path

_PACKAGE_ROOT = str(Path(__file__).parents[2] / "python_api_package")
if _PACKAGE_ROOT not in sys.path:
    sys.path.insert(0, _PACKAGE_ROOT)
