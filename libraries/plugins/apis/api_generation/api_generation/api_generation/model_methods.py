"""Hand-written methods of generated public models - behaviour kept from the hand-written models of schemas."""

from __future__ import annotations

import ast
from typing import TYPE_CHECKING, Final

if TYPE_CHECKING:
    from collections.abc import Mapping
    from pathlib import Path

MODEL_METHODS: Final[dict[str, str]] = {
    "AppStatusStatusItem": '''
    def __eq__(self, other: object) -> bool:
        """
        Compare with another status item or with the name of a status.

        A status item equals its name, so a status is checked like `"chain API ready" in app_status.statuses`.
        Like other models, a status item is not hashable.
        """
        if isinstance(other, str):
            return self.status == other
        if type(other) is not type(self):
            return NotImplemented
        return all(getattr(self, name) == getattr(other, name) for name in self.__dataclass_fields__)
''',
}
"""Source of methods (indented as in a class body) by names of generated models they are added to."""


def add_model_methods(files: list[Path], methods: Mapping[str, str] = MODEL_METHODS) -> None:
    """
    Append the methods to the bodies of the generated models defining them (in any of the files).

    Raises:
        ValueError: When a model of `methods` is not generated (e.g. renamed in openapi.json).
    """
    added: set[str] = set()
    for file in files:
        lines = file.read_text().splitlines(keepends=True)
        classes = [
            node for node in ast.parse("".join(lines)).body if isinstance(node, ast.ClassDef) and node.name in methods
        ]
        for node in sorted(classes, key=lambda node: node.lineno, reverse=True):
            assert node.end_lineno is not None
            lines.insert(node.end_lineno, methods[node.name])
            added.add(node.name)
        if classes:
            file.write_text("".join(lines))
    if missing := sorted(set(methods) - added):
        raise ValueError(f"Models {missing} with hand-written methods are not generated")
