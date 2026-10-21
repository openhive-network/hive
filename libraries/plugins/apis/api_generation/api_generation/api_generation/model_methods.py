"""Hand-written methods of generated public models - behaviour kept from the hand-written models of schemas."""

from __future__ import annotations

import ast
from typing import TYPE_CHECKING, Final

if TYPE_CHECKING:
    from collections.abc import Mapping
    from pathlib import Path

MODEL_METHODS: Final[dict[str, dict[str, str]]] = {
    "AppStatusStatusItem": {"__eq__": "hiveio_api._model_methods.status_item_eq"},
}
"""
Hand-written methods by names of generated models they are assigned to - dotted paths of the functions (defined in
`hiveio_api._model_methods`) by names of the methods.
"""


def add_model_methods(files: list[Path], methods: Mapping[str, Mapping[str, str]] = MODEL_METHODS) -> None:
    """
    Assign the hand-written methods in the bodies of the generated models defining them (in any of the files).

    Each method is a line of the class body (e.g. `__eq__ = status_item_eq`), its function is imported by the module.

    Args:
        files: Generated modules of public models.
        methods: Dotted paths of functions by names of methods, by names of models.

    Raises:
        ValueError: When a model of `methods` is not generated (e.g. renamed in openapi.json).
    """
    added: set[str] = set()
    for file in files:
        lines = file.read_text().splitlines(keepends=True)
        module = ast.parse("".join(lines))
        classes = [node for node in module.body if isinstance(node, ast.ClassDef) and node.name in methods]
        if not classes:
            continue
        functions: set[str] = set()
        for node in sorted(classes, key=lambda node: node.lineno, reverse=True):
            assert node.end_lineno is not None
            assignments = methods[node.name].items()
            lines.insert(
                node.end_lineno,
                "".join(f"\n    {name} = {function.rpartition('.')[2]}\n" for name, function in assignments),
            )
            functions.update(function for _, function in assignments)
            added.add(node.name)
        imports = [node for node in module.body if isinstance(node, ast.Import | ast.ImportFrom)]
        last_import = max((node.end_lineno or node.lineno for node in imports), default=0)  # classes follow imports
        lines.insert(
            last_import,
            "".join(f"from {path} import {name}\n" for path, _, name in sorted(f.rpartition(".") for f in functions)),
        )
        file.write_text("".join(lines))
    if missing := sorted(set(methods) - added):
        raise ValueError(f"Models {missing} with hand-written methods are not generated")
