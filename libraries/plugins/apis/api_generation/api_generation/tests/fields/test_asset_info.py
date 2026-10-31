from __future__ import annotations

import pytest
from hiveio_api import HBD_ASSET, HIVE_ASSET, VESTS_ASSET, Asset, AssetInfo
from hiveio_api.fields.public import KNOWN_ASSETS, asset_info_of_nai, asset_info_of_symbol

# the code repeating NAIs or symbols of kinds of assets is checked by tests/test_kinds_of_assets.py


def test_information_is_equal_whatever_its_network_and_to_both_symbols_of_the_kind() -> None:
    # ARRANGE
    testnet = HIVE_ASSET.of_network(testnet=True)

    # ACT & ASSERT
    assert testnet == HIVE_ASSET
    assert hash(testnet) == hash(HIVE_ASSET)
    assert HIVE_ASSET == "HIVE"
    assert HIVE_ASSET == "TESTS"
    assert "TBD" == HBD_ASSET  # noqa: SIM300  # the information on the right - reflected `__eq__`
    assert HIVE_ASSET != "TBD"
    assert HIVE_ASSET != HBD_ASSET
    assert HIVE_ASSET.nai != HIVE_ASSET


def test_information_is_displayed_with_the_symbol_of_its_network() -> None:
    # ACT & ASSERT
    assert (str(HBD_ASSET), f"{HBD_ASSET.of_network(testnet=True)}") == ("HBD", "TBD")
    assert str(VESTS_ASSET.of_network(testnet=True)) == "VESTS"


def test_symbol_of_the_network_is_given_whatever_the_network_of_the_information() -> None:
    # ARRANGE
    testnet = HBD_ASSET.of_network(testnet=True)

    # ACT & ASSERT
    assert (HBD_ASSET.get_symbol(), HBD_ASSET.get_symbol(testnet=True)) == ("HBD", "TBD")
    assert (testnet.get_symbol(), testnet.get_symbol(testnet=True)) == ("HBD", "TBD")


def test_information_is_found_by_nai() -> None:
    # ACT & ASSERT
    assert [asset_info_of_nai(info.nai) for info in KNOWN_ASSETS] == list(KNOWN_ASSETS)
    assert asset_info_of_nai("@@000000099") is None


@pytest.mark.parametrize(
    ("symbol", "info", "testnet"),
    [("HIVE", HIVE_ASSET, False), ("TESTS", HIVE_ASSET, True), ("TBD", HBD_ASSET, True), ("VESTS", VESTS_ASSET, False)],
)
def test_information_is_found_by_symbol_of_either_network(symbol: str, info: AssetInfo, testnet: bool) -> None:
    # ACT
    found = asset_info_of_symbol(symbol)

    # ASSERT
    assert found is not None
    assert found == info
    assert found.testnet is testnet
    assert str(found) == symbol


def test_information_of_unknown_symbols_is_not_found() -> None:
    # ACT & ASSERT
    assert asset_info_of_symbol("STEEM") is None


def test_every_kind_of_assets_has_its_kind_in_asset() -> None:
    # ACT
    kinds = [Asset.kind_of(info) for info in KNOWN_ASSETS]

    # ASSERT
    assert kinds == [Asset.hive, Asset.hbd, Asset.vests]
    assert [kind.info for kind in kinds] == list(KNOWN_ASSETS)


def test_information_of_other_kinds_has_no_kind_in_asset() -> None:
    # ARRANGE
    info = AssetInfo(nai="@@000000099", precision=2, symbol=("ABC", "TABC"))

    # ACT & ASSERT
    with pytest.raises(ValueError, match="No kind of assets"):
        Asset.kind_of(info)


@pytest.mark.parametrize(
    ("asset", "info", "displayed"),
    [
        (Asset.hive.satoshi(1), HIVE_ASSET, "HIVE"),
        (Asset("1.000 TBD"), HBD_ASSET, "TBD"),
        (Asset.vests.tokens(1, testnet=True), VESTS_ASSET, "VESTS"),
    ],
)
def test_assets_give_information_about_their_kind(asset: Asset, info: AssetInfo, displayed: str) -> None:
    # ACT
    token = asset.token()

    # ASSERT
    assert token == info
    assert str(token) == displayed


def test_assets_of_other_kinds_have_no_information() -> None:
    # ARRANGE
    asset = Asset({"amount": "7", "precision": 2, "nai": "@@000000099"})

    # ACT & ASSERT
    with pytest.raises(ValueError, match="no known kind"):
        asset.token()
