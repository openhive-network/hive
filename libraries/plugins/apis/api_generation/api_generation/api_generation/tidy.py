"""Clean up generated code: remove unused imports, sort imports and format with ruff."""

from __future__ import annotations

import subprocess
from typing import TYPE_CHECKING, Final

from ruff.__main__ import find_ruff_bin  # type: ignore[import-untyped]

if TYPE_CHECKING:
    from pathlib import Path

LINT_RULES: Final[str] = "F401,I"
"""Unused imports and import sorting."""


def tidy_with_ruff(*paths: Path) -> None:
    """
    Remove unused imports, sort imports and format the given files / directories in place.

    The ruff configuration of the repository (hive/pyproject.toml) applies, so the result matches what linters and
    editors report. Generated files are git-ignored, hence `--no-respect-gitignore`.
    """
    targets = [str(path) for path in paths if path.exists()]
    if not targets:
        return

    ruff = find_ruff_bin()
    subprocess.run(
        [ruff, "check", "--fix", "--exit-zero", "--quiet", "--no-respect-gitignore", "--select", LINT_RULES, *targets],
        check=True,
    )
    subprocess.run([ruff, "format", "--quiet", "--no-respect-gitignore", *targets], check=True)
