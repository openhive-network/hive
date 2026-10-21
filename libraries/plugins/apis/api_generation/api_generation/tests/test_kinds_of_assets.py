from __future__ import annotations

from pathlib import Path
from typing import Final

import pytest

from api_generation.generate_operations import asset_info, asset_type
from schemas.fields.public import HBD_ASSET, HIVE_ASSET, VESTS_ASSET
from tests.generated_package import TEMPLATE_DIR
from tests.kinds_of_assets import literals_of_kinds_of_assets

GENERATOR_DIR: Final[Path] = Path(__file__).parents[1] / "api_generation"


@pytest.mark.parametrize("directory", [GENERATOR_DIR, TEMPLATE_DIR], ids=["generator", "template"])
def test_kinds_of_assets_are_taken_from_schemas(directory: Path) -> None:
    # ACT
    literals = [
        literal for module in sorted(directory.rglob("*.py")) for literal in literals_of_kinds_of_assets(module)
    ]

    # ASSERT
    assert not literals, "NAIs and symbols of assets have to be taken from schemas (`HIVE_ASSET`, ...)"


def test_symbols_of_the_description_of_operations_are_found_in_schemas() -> None:
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
