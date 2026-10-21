"""
Check of generated models - assets are always declared in openapi.json (`x-hive-type`), never generated as models.

Assets are the user-friendly `schemas.Asset` (public models) or validating assets of schemas (validation models).
A model with fields `amount`, `nai` and `precision` is an asset whose schema lacks `x-hive-type` - generation fails,
so the definition gets fixed instead of the API returning a model unlike all other assets.
"""

from __future__ import annotations

import ast
from typing import TYPE_CHECKING, Final

if TYPE_CHECKING:
    from pathlib import Path

ASSET_FIELDS: Final[frozenset[str]] = frozenset({"amount", "nai", "precision"})
"""Fields of the NAI form of assets."""


class UndeclaredAssetError(ValueError):
    """A generated model is an asset - openapi.json lacks `x-hive-type` of an asset there."""


def check_no_undeclared_assets(description_file: Path, api: str) -> None:
    """
    Raise when a model generated into the description module of the API is an asset.

    Args:
        description_file: Generated module of models.
        api: Name of the API (or of the package) of the module - told by the error.

    Raises:
        UndeclaredAssetError: When a model has the fields of the NAI form of assets.
    """
    tree = ast.parse(description_file.read_text(encoding="utf-8"), filename=str(description_file))
    undeclared = sorted(
        node.name
        for node in tree.body
        if isinstance(node, ast.ClassDef)
        and node.bases  # every generated model derives from the model base class
        and ASSET_FIELDS
        <= {
            statement.target.id
            for statement in node.body
            if isinstance(statement, ast.AnnAssign) and isinstance(statement.target, ast.Name)
        }
    )
    if undeclared:
        raise UndeclaredAssetError(
            f"Undeclared asset detected in models of `{api}`: {', '.join(undeclared)} - fields "
            f"{', '.join(f'`{field}`' for field in sorted(ASSET_FIELDS))} make an asset. Fix openapi.json: declare "
            "the asset with `x-hive-type` (e.g. `AssetHive`, `AnyAsset`, `LegacyAssetHbd`)!"
        )
