from __future__ import annotations

from pathlib import Path
from typing import Final

import pytest

from tests.generated_package import TEMPLATE_DIR, require_generated_package

require_generated_package()

from api_generation.generate_operations import asset_info, asset_type  # noqa: E402
from hiveio_api.fields.public import HBD_ASSET, HIVE_ASSET, VESTS_ASSET  # noqa: E402
from tests.kinds_of_assets import literals_of_kinds_of_assets  # noqa: E402

GENERATOR_DIR: Final[Path] = Path(__file__).parents[1] / "api_generation"
DEFINITION: Final[Path] = TEMPLATE_DIR / "fields" / "public" / "_asset_info.py"
"""The only definition of NAIs, precisions and symbols of kinds of assets."""


@pytest.mark.parametrize("directory", [GENERATOR_DIR, TEMPLATE_DIR], ids=["generator", "template"])
def test_kinds_of_assets_are_taken_from_their_definition(directory: Path) -> None:
    # ACT
    literals = [
        literal
        for module in sorted(directory.rglob("*.py"))
        if module != DEFINITION
        for literal in literals_of_kinds_of_assets(module)
    ]

    # ASSERT
    assert not literals, "NAIs and symbols of assets have to be taken from `hiveio_api.fields.public` (`HIVE_ASSET`, ...)"
    assert literals_of_kinds_of_assets(DEFINITION), "the check has to find the definitions"


def test_symbols_of_the_description_of_operations_are_found_in_kinds_of_assets() -> None:
    # ACT & ASSERT - symbols of `x-hive-asset-symbols` and of defaults (cmake/operations_description_generator.cpp)
    assert [asset_info(symbol) for symbol in ("HIVE", "HBD", "VESTS")] == [HIVE_ASSET, HBD_ASSET, VESTS_ASSET]
    with pytest.raises(ValueError, match="Unknown symbol"):
        asset_info("STEEM")


@pytest.mark.parametrize(
    ("symbols", "hf26", "legacy"),
    [
        (["HBD"], "AssetHbd", "LegacyAssetHbd"),
        (["HIVE", "VESTS"], "AssetHiveOrVests", "LegacyAssetHiveOrVests"),
        (["VESTS", "HBD", "HIVE"], "AnyAsset", "LegacyAnyAsset"),
    ],
)
def test_types_of_assets_are_chosen_by_kinds_of_assets(symbols: list[str], hf26: str, legacy: str) -> None:
    # ACT & ASSERT
    assert (asset_type(symbols, "hf26"), asset_type(symbols, "legacy")) == (hf26, legacy)
