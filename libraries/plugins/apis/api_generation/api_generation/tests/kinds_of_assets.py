"""
Check of the code for NAIs and symbols of kinds of assets - schemas defines them only once (`HIVE_ASSET`, ...).

The generator and hiveio_api refer to the information about kinds of assets of schemas - they never repeat NAIs, symbols or
precisions of assets.
"""

from __future__ import annotations

import ast
import re
from typing import TYPE_CHECKING, Final

from schemas.fields.public import KNOWN_ASSETS

if TYPE_CHECKING:
    from pathlib import Path

NAI_PATTERN: Final[re.Pattern[str]] = re.compile(r"@@[0-9]{9}")
SYMBOLS: Final[frozenset[str]] = frozenset(symbol for info in KNOWN_ASSETS for symbol in info.symbol)


def literals_of_kinds_of_assets(path: Path) -> list[str]:
    """String literals of the code (not documentation) holding NAIs or symbols of kinds of assets."""
    tree = ast.parse(path.read_text(), filename=str(path))
    documentation = {
        id(node.value)
        for node in ast.walk(tree)
        if isinstance(node, ast.Expr) and isinstance(node.value, ast.Constant) and isinstance(node.value.value, str)
    }
    return [
        f"{path}:{node.lineno}: {node.value!r}"
        for node in ast.walk(tree)
        if isinstance(node, ast.Constant)
        and isinstance(node.value, str)
        and id(node) not in documentation
        and (NAI_PATTERN.search(node.value) is not None or node.value in SYMBOLS)
    ]
