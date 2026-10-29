"""
Information about kinds of assets of Hive (precision, NAI, symbols) - the only definition of them in hiveio_api.

Everything else - kinds of the user-friendly `Asset` (`Asset.hive`), validating types of assets (`AssetHive`, ...),
models generated in hiveio_api - refers to `HIVE_ASSET`, `HBD_ASSET` and `VESTS_ASSET` and finds kinds by NAI or symbol
with `asset_info_of_nai` / `asset_info_of_symbol`; NAIs, symbols and precisions are not repeated anywhere else.

This module must stay import-light (no msgspec, no other hiveio_api modules) - generated public models use it.
"""

from __future__ import annotations

from dataclasses import dataclass, replace
from typing import Final, Literal, get_args

__all__ = [
    "HBD_ASSET",
    "HBD_ASSET_NAI",
    "HBD_ASSET_PRECISION",
    "HIVE_ASSET",
    "HIVE_ASSET_NAI",
    "HIVE_ASSET_PRECISION",
    "KNOWN_ASSETS",
    "VESTS_ASSET",
    "VESTS_ASSET_NAI",
    "VESTS_ASSET_PRECISION",
    "AssetInfo",
    "asset_info_of_nai",
    "asset_info_of_symbol",
]

# NAIs and precisions of the kinds as literal types - fields of validating assets of the NAI form are of these types,
# the information about the kinds (`HIVE_ASSET`, ...) takes their values
HIVE_ASSET_NAI = Literal["@@000000021"]
HBD_ASSET_NAI = Literal["@@000000013"]
VESTS_ASSET_NAI = Literal["@@000000037"]
HIVE_ASSET_PRECISION = Literal[3]
HBD_ASSET_PRECISION = Literal[3]
VESTS_ASSET_PRECISION = Literal[6]


@dataclass(frozen=True)
class AssetInfo:
    """
    Information about a kind of asset - its precision, NAI and symbols of the legacy form on the mainnet and the testnet.

    The information is displayed (`str`) with the symbol of its network (`testnet`). It is equal to the information of
    the same kind whatever the network (NAIs are compared) and to both symbols of the kind (`HIVE_ASSET == "TESTS"`) -
    hashes of symbols differ from the hash of the information, do not mix them as keys.
    """

    precision: int
    nai: str
    symbol: tuple[str, str]
    """Symbols of the legacy form on the mainnet and on the testnet (`("HIVE", "TESTS")`)."""
    testnet: bool = False
    """The information is displayed with the symbol of the testnet."""

    def get_symbol(self, testnet: bool = False) -> str:
        """Symbol of the legacy form on the network (`str` gives the symbol of the network of the information)."""
        return self.symbol[int(testnet)]

    def of_network(self, *, testnet: bool) -> AssetInfo:
        """The same information displayed with the symbol of the network."""
        return self if testnet == self.testnet else replace(self, testnet=testnet)

    def __str__(self) -> str:
        return self.get_symbol(self.testnet)

    def __eq__(self, other: object) -> bool:
        if isinstance(other, str):
            return other in self.symbol
        if isinstance(other, AssetInfo):
            return self.nai == other.nai
        return NotImplemented

    def __hash__(self) -> int:
        return hash(self.nai)


HIVE_ASSET: Final[AssetInfo] = AssetInfo(
    precision=get_args(HIVE_ASSET_PRECISION)[0], nai=get_args(HIVE_ASSET_NAI)[0], symbol=("HIVE", "TESTS")
)
HBD_ASSET: Final[AssetInfo] = AssetInfo(
    precision=get_args(HBD_ASSET_PRECISION)[0], nai=get_args(HBD_ASSET_NAI)[0], symbol=("HBD", "TBD")
)
VESTS_ASSET: Final[AssetInfo] = AssetInfo(
    precision=get_args(VESTS_ASSET_PRECISION)[0], nai=get_args(VESTS_ASSET_NAI)[0], symbol=("VESTS", "VESTS")
)
KNOWN_ASSETS: Final[tuple[AssetInfo, ...]] = (HIVE_ASSET, HBD_ASSET, VESTS_ASSET)
"""Information about all kinds of assets of Hive."""


def asset_info_of_nai(nai: str) -> AssetInfo | None:
    """Information about the kind of the NAI (of the mainnet), None for NAIs of other kinds."""
    return next((info for info in KNOWN_ASSETS if info.nai == nai), None)


def asset_info_of_symbol(symbol: str) -> AssetInfo | None:
    """Information about the kind of the symbol - of the network of the symbol (`TESTS` - testnet), None for others."""
    info = next((info for info in KNOWN_ASSETS if info == symbol), None)
    return None if info is None else info.of_network(testnet=symbol != info.get_symbol())
