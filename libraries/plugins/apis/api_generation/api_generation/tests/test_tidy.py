from __future__ import annotations

import textwrap
from typing import TYPE_CHECKING

from api_generation.tidy import tidy_with_ruff

if TYPE_CHECKING:
    from pathlib import Path


def test_tidy_removes_unused_and_sorts_imports(tmp_path: Path) -> None:
    # ARRANGE
    module = tmp_path / "generated.py"
    module.write_text(
        textwrap.dedent(
            """
            from __future__ import annotations
            from typing import TypeAlias, Any
            from dataclasses import dataclass
            from os import path
            Alias: TypeAlias = dict[str, Any]
            @dataclass
            class Model:
                value: Alias
            """
        )
    )

    # ACT
    tidy_with_ruff(module)

    # ASSERT
    source = module.read_text()
    assert "from os import path" not in source
    assert source.index("from dataclasses import dataclass") < source.index("from typing import Any, TypeAlias")


def test_tidy_sorts_dunder_all_and_strips_trailing_whitespace_in_docstrings(tmp_path: Path) -> None:
    # ARRANGE
    module = tmp_path / "generated.py"
    module.write_text(
        'from __future__ import annotations\n\n__all__ = ["b", "a"]\n\na = 1\nb = 2\n\n\nclass Model:\n'
        '    """\n    Description from openapi.json   \n    """\n'
    )

    # ACT
    tidy_with_ruff(module)

    # ASSERT
    source = module.read_text()
    assert '__all__ = ["a", "b"]' in source
    assert "openapi.json\n" in source
