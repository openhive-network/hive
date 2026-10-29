from __future__ import annotations

import copy
import pickle
from dataclasses import asdict, dataclass
from decimal import Decimal
from typing import TYPE_CHECKING, Final

import pytest
from hiveio_api import HBD_ASSET, Asset
from hiveio_api.validation.encoders import get_hf26_encoder

if TYPE_CHECKING:
    from collections.abc import Callable

    from hiveio_api.fields.public import AssetFactory, AssetKind

HIVE_NAI: Final[dict[str, object]] = {"amount": "1001", "precision": 3, "nai": "@@000000021"}


@pytest.mark.parametrize(
    ("asset", "legacy", "nai"),
    [
        (Asset.hive.satoshi(1001), "1.001 HIVE", HIVE_NAI),
        (Asset.hbd.tokens(1.001), "1.001 HBD", {"amount": "1001", "precision": 3, "nai": "@@000000013"}),
        (Asset.hbd.tokens("0.5"), "0.500 HBD", {"amount": "500", "precision": 3, "nai": "@@000000013"}),
        (Asset.vests.tokens(Decimal(2)), "2.000000 VESTS", {"amount": "2000000", "precision": 6, "nai": "@@000000037"}),
        (Asset.hive.factory(amount="1001", nai="@@000000021", precision=3), "1.001 HIVE", HIVE_NAI),
        (Asset.hive.factory(amount=1001), "1.001 HIVE", HIVE_NAI),
        (Asset.hive.satoshi(-5), "-0.005 HIVE", {"amount": "-5", "precision": 3, "nai": "@@000000021"}),
    ],
)
def test_kinds_create_assets_in_the_nai_form(asset: Asset, legacy: str, nai: dict[str, object]) -> None:
    # ACT & ASSERT
    assert asset.is_legacy is False
    assert asset.to_builtins() == nai
    assert asset.as_nai() == nai
    assert asset.as_legacy() == legacy
    assert str(asset) == legacy


@pytest.mark.parametrize(
    "value",
    [
        "1.001 HIVE",
        "1.001 TESTS",
        "0.000 TBD",
        "-12.345 HBD",
        "1.000000 VESTS",
        {"amount": "1001", "precision": 3, "nai": "@@000000021"},
        {"amount": "0", "precision": 6, "nai": "@@000000037"},
        {"amount": "7", "precision": 2, "nai": "@@000000099"},
    ],
)
def test_assets_built_from_json_are_serialized_back_to_the_same_form(value: str | dict[str, object]) -> None:
    # ACT
    asset = Asset.from_builtins(value)

    # ASSERT
    assert asset.is_legacy is isinstance(value, str)
    assert asset.to_builtins() == value
    assert Asset(value) == asset


def test_amount_given_as_integer_in_the_nai_form_is_serialized_as_string() -> None:
    # ACT
    asset = Asset({"amount": 1001, "precision": 3, "nai": "@@000000021"})

    # ASSERT
    assert asset.to_builtins() == HIVE_NAI


@pytest.mark.parametrize(
    ("asset", "testnet", "expected"),
    [
        (Asset.hive.satoshi(1001), None, "1.001 HIVE"),
        (Asset.hive.satoshi(1001), True, "1.001 TESTS"),
        (Asset.hbd.satoshi(1001), True, "1.001 TBD"),
        (Asset.vests.satoshi(1), True, "0.000001 VESTS"),
        (Asset("1.001 TESTS"), None, "1.001 TESTS"),
        (Asset("1.001 TESTS"), False, "1.001 HIVE"),
        (Asset("1.001 HIVE"), True, "1.001 TESTS"),
        (Asset.hbd.tokens(1.001, testnet=True), None, "1.001 TBD"),
        (Asset.hbd.tokens(1.001, testnet=True), False, "1.001 HBD"),
    ],
)
def test_legacy_form_uses_symbol_of_the_network(asset: Asset, testnet: bool | None, expected: str) -> None:
    # ACT & ASSERT
    assert asset.as_legacy(testnet=testnet) == expected


def test_network_of_an_asset_is_only_displayed() -> None:
    # ARRANGE
    testnet_asset = Asset.hive.tokens(10, testnet=True)

    # ACT & ASSERT
    assert str(testnet_asset) == "10.000 TESTS"
    assert str(testnet_asset + Asset.hive.tokens(1)) == "11.000 TESTS", "operations keep the network of the left one"
    assert testnet_asset.to_builtins() == {"amount": "10000", "precision": 3, "nai": "@@000000021"}
    assert testnet_asset == Asset.hive.tokens(10)
    assert testnet_asset == "10.000 HIVE"
    assert "10.000 TESTS" == testnet_asset  # noqa: SIM300  # the asset on the right - reflected `__eq__`


def test_members_of_the_legacy_form() -> None:
    # ARRANGE
    asset = Asset.hbd.tokens("1.5", testnet=True)

    # ACT & ASSERT
    assert (str(asset.token()), str(asset.token(testnet=False)), asset.pretty_amount(), asset.as_float()) == (
        "TBD",
        "HBD",
        "1.500",
        1.5,
    )
    assert asset.as_serialized_nai() == asset.as_nai()
    assert Asset({"amount": "7", "precision": 2, "nai": "@@000000099"}).pretty_amount() == "0.07"


def test_assets_are_compared_by_value_whatever_their_form() -> None:
    # ARRANGE
    forms = [
        Asset.hive.satoshi(1001),
        Asset("1.001 HIVE"),
        Asset("1.001 TESTS"),
        Asset({"amount": 1001, "precision": 3, "nai": "@@000000021"}),
    ]

    # ACT & ASSERT
    assert all(asset == forms[0] for asset in forms)
    assert len(set(forms)) == 1
    assert Asset.hive.satoshi(1001) != Asset.hbd.satoshi(1001)
    assert Asset.hive.satoshi(1001) != Asset.hive.satoshi(1000)


@pytest.mark.parametrize(
    "json_of_asset",
    [
        "1.001 HIVE",
        "1.001 TESTS",
        {"amount": "1001", "precision": 3, "nai": "@@000000021"},
        {"amount": 1001, "precision": 3, "nai": "@@000000021"},
    ],
)
def test_assets_are_equal_to_json_of_the_same_asset(json_of_asset: str | dict[str, object]) -> None:
    # ARRANGE
    asset = Asset.hive.satoshi(1001)

    # ACT & ASSERT
    assert asset == json_of_asset
    assert json_of_asset == asset
    assert not asset != json_of_asset  # noqa: SIM202 - `!=` is checked on purpose


@pytest.mark.parametrize(
    "other",
    ["1.000 HIVE", "1.001 HBD", "1.001", "", "not an asset", {"amount": "1001"}, {}, 1000, True, None, [1001, 3]],
)
def test_assets_are_not_equal_to_other_values(other: object) -> None:
    # ACT & ASSERT
    assert Asset.hive.satoshi(1001) != other
    assert other != Asset.hive.satoshi(1001)


def test_assets_are_compared_with_integers_of_the_smallest_units() -> None:
    # ARRANGE
    satoshi = 1001
    asset = Asset.hive.satoshi(satoshi)

    # ACT & ASSERT
    assert asset == satoshi
    assert satoshi == asset
    assert asset > 0
    assert asset >= satoshi
    assert asset < satoshi + 1
    assert asset <= satoshi
    assert 0 < asset  # noqa: SIM300 - the reflected comparison is checked on purpose
    assert not Asset.hbd.satoshi(0) > 0
    with pytest.raises(TypeError):
        _ = asset < True


def test_copy_of_asset_takes_another_amount_and_keeps_the_kind_form_and_network() -> None:
    # ARRANGE
    asset = Asset("1.001 TESTS")

    # ACT
    copies = [asset.copy(), asset.copy(amount=5), asset.copy(amount=Asset.hive.satoshi(7))]

    # ASSERT
    assert [copy_.amount for copy_ in copies] == [1001, 5, 7]
    assert all(copy_.is_hive() and copy_.as_legacy().endswith(" TESTS") and copy_.is_legacy for copy_ in copies)
    with pytest.raises(TypeError):
        asset.copy(amount=Asset.hbd.satoshi(7))
    with pytest.raises(TypeError):
        asset.copy(amount="5")  # type: ignore[arg-type]


@dataclass(frozen=True)
class MessageOfAsset:
    """Object with members of the NAI form, as protobuf assets of wax."""

    amount: str
    precision: int
    nai: str


@pytest.mark.parametrize(
    "other_form",
    [
        "1.001 TESTS",
        {"amount": "1001", "precision": 3, "nai": "@@000000021"},
        MessageOfAsset(amount="1001", precision=3, nai="@@000000021"),
    ],
    ids=["legacy form", "nai form", "message of asset"],
)
def test_assets_are_equal_to_other_forms_of_the_same_asset(other_form: object) -> None:
    # ARRANGE
    asset = Asset.hive.satoshi(1001)

    # ACT & ASSERT
    assert asset == other_form
    assert other_form == asset
    assert asset != Asset.hive.satoshi(1000)
    assert Asset.from_asset_like(other_form) == asset


def test_arithmetic_and_ordering_take_other_forms_of_assets() -> None:
    # ARRANGE
    asset = Asset.hive.satoshi(1000)

    # ACT & ASSERT
    assert asset + "0.001 HIVE" == Asset.hive.satoshi(1001)
    assert "0.001 HIVE" + asset == Asset.hive.satoshi(1001)
    assert "2.000 HIVE" - asset == Asset.hive.satoshi(1000)
    assert asset - MessageOfAsset(amount="1", precision=3, nai="@@000000021") == Asset.hive.satoshi(999)
    assert asset < "1.001 TESTS"
    assert asset >= {"amount": "1000", "precision": 3, "nai": "@@000000021"}
    assert "0.999 HIVE" < asset  # noqa: SIM300 - the reflected comparison is checked


def test_from_asset_like_gives_none_for_values_which_are_not_assets() -> None:
    # ACT & ASSERT
    assert [Asset.from_asset_like(value) for value in ("1.00 HIVE", {"nai": "x"}, 1, None, object())] == [None] * 5


@dataclass(frozen=True)
class ConvertedToNai:
    """Object converting itself to the NAI form."""

    nai_form: object

    def as_nai(self) -> object:
        return self.nai_form


def test_from_asset_like_takes_objects_converting_themselves_to_the_nai_form() -> None:
    # ARRANGE
    converted = ConvertedToNai({"amount": "1001", "precision": 3, "nai": "@@000000021"})

    # ACT & ASSERT
    assert Asset.from_asset_like(converted) == Asset.hive.satoshi(1001)
    assert Asset.from_asset_like(ConvertedToNai("1.001 HIVE")) is None


def test_members_of_the_asset() -> None:
    # ACT
    asset = Asset("1.001 TBD")

    # ASSERT
    assert (asset.amount, asset.tokens, asset.precision, asset.nai, asset.symbol) == (
        1001,
        Decimal("1.001"),
        3,
        "@@000000013",
        "HBD",
    )


@pytest.mark.parametrize(
    ("asset", "kind"),
    [
        (Asset("1.000 TESTS"), "hive"),
        (Asset.hive.tokens(1), "hive"),
        (Asset("1.000 TBD"), "hbd"),
        (Asset.vests.satoshi(1), "vests"),
    ],
)
def test_assets_tell_their_kind(asset: Asset, kind: str) -> None:
    # ACT
    kinds = {"hive": asset.is_hive(), "hbd": asset.is_hbd(), "vests": asset.is_vests()}

    # ASSERT
    assert kinds == {name: name == kind for name in kinds}


@pytest.mark.parametrize("kind_or_factory", [Asset.hbd, Asset.hbd.satoshi, Asset.hbd.tokens])
def test_kinds_and_factories_of_assets_give_information_about_the_kind(
    kind_or_factory: AssetKind | AssetFactory,
) -> None:
    # ACT
    mainnet, testnet = kind_or_factory.token(), kind_or_factory.token(testnet=True)

    # ASSERT
    assert mainnet == testnet == HBD_ASSET
    assert (str(mainnet), str(testnet)) == ("HBD", "TBD")


def test_factory_of_assets_stands_for_its_kind() -> None:
    # ARRANGE
    currency = Asset.hive.satoshi

    # ACT
    asset = currency(amount=5)

    # ASSERT
    assert asset == Asset.hive.tokens("0.005")
    assert str(currency.token()) == str(asset.token()) == "HIVE"
    assert currency == Asset.hive.satoshi
    assert currency not in (Asset.hbd.satoshi, Asset.hive.tokens)
    assert repr(currency) == "Asset.hive.satoshi"
    assert repr(Asset.vests.tokens) == "Asset.vests.tokens"


def test_arithmetic_keeps_the_kind_and_the_form_of_the_left_operand() -> None:
    # ARRANGE
    legacy = Asset("1.000 TESTS")

    # ACT
    results = [legacy + Asset.hive.satoshi(5), legacy - Asset.hive.satoshi(5), legacy * 2, 3 * legacy, -legacy]

    # ASSERT
    assert [result.to_builtins() for result in results] == [
        "1.005 TESTS",
        "0.995 TESTS",
        "2.000 TESTS",
        "3.000 TESTS",
        "-1.000 TESTS",
    ]
    assert abs(-legacy) == legacy
    assert Asset.hive.satoshi(1) > Asset.hive.satoshi(0)
    assert Asset.hive.satoshi(1) >= Asset("0.001 HIVE")
    assert Asset.hive.satoshi(0) < Asset.hive.satoshi(1)
    assert Asset.hive.satoshi(0) <= Asset.hive.satoshi(0)


def test_assets_are_true_also_of_zero_amount() -> None:
    # ACT & ASSERT - as assets were before (`if fee:` checks a given fee, `fee.amount` its amount)
    assert Asset.hive.satoshi(0)
    assert Asset.hive.satoshi(1)


def test_assets_are_summed() -> None:
    # ACT
    total = sum([Asset("0.002 TESTS"), Asset.hive.satoshi(1), "0.003 HIVE"])  # type: ignore[list-item]

    # ASSERT
    assert isinstance(total, Asset)
    assert total.to_builtins() == "0.006 TESTS"


def test_assets_are_divided_by_integers_and_by_assets_of_the_same_kind() -> None:
    # ARRANGE
    asset = Asset("0.010 TESTS")

    # ACT & ASSERT
    assert (asset // 3).to_builtins() == "0.003 TESTS"
    assert (-asset // 3) == Asset.hive.satoshi(-4)
    assert (asset / 3).to_builtins() == "0.003 TESTS"
    assert (-asset / 3) == Asset.hive.satoshi(-3)
    assert (asset / -3) == Asset.hive.satoshi(-3)
    assert asset / Asset.hive.satoshi(4) == Decimal("2.5")
    assert asset / "0.020 HIVE" == Decimal("0.5")
    quotient = asset
    quotient //= 2
    assert quotient == Asset.hive.satoshi(5)


@pytest.mark.parametrize(
    "operation",
    [
        lambda: Asset.hive.satoshi(1) // 0,
        lambda: Asset.hive.satoshi(1) / 0,
        lambda: Asset.hive.satoshi(1) / Asset.hive.satoshi(0),
    ],
)
def test_division_by_zero_is_rejected(operation: object) -> None:
    # ACT & ASSERT
    with pytest.raises(ZeroDivisionError):
        operation()  # type: ignore[operator]


@pytest.mark.parametrize(
    "operation",
    [
        lambda: Asset.hive.satoshi(1) + Asset.hbd.satoshi(1),
        lambda: Asset.hive.satoshi(1) - Asset.vests.satoshi(1),
        lambda: Asset.hive.satoshi(1) < Asset.hbd.satoshi(1),
        lambda: Asset.hive.satoshi(1) + 1,
        lambda: 1 + Asset.hive.satoshi(1),
        lambda: False + Asset.hive.satoshi(1),
        lambda: Asset.hive.satoshi(1) + 0,
        lambda: Asset.hive.satoshi(1) * 1.5,  # type: ignore[operator]
        lambda: Asset.hive.satoshi(1) // 1.5,  # type: ignore[operator]
        lambda: Asset.hive.satoshi(1) / Asset.hbd.satoshi(1),
        lambda: Asset.hive.satoshi(1) / 1.5,  # type: ignore[operator]
        lambda: Asset.hive.satoshi(1) / True,
    ],
)
def test_operations_on_assets_of_different_kinds_are_rejected(operation: object) -> None:
    # ACT & ASSERT
    with pytest.raises(TypeError):
        operation()  # type: ignore[operator]


@pytest.mark.parametrize(
    "value",
    [
        "1.00 HIVE",
        "1.001 STEEM",
        "1.001HIVE",
        "1,001 HIVE",
        "",
        {"amount": "1.5", "precision": 3, "nai": "@@000000021"},
        {"amount": "1001", "precision": 6, "nai": "@@000000021"},
        {"amount": "1001", "precision": 3},
        {"amount": "1001", "precision": 3, "nai": "@@000000021", "symbol": "HIVE"},
        {"amount": True, "precision": 3, "nai": "@@000000021"},
        {"amount": "1_001", "precision": 3, "nai": "@@000000021"},
        {"amount": " 1001", "precision": 3, "nai": "@@000000021"},
        {"amount": "1001\n", "precision": 3, "nai": "@@000000021"},
        {"amount": "5", "precision": -3, "nai": "@@000000099"},
        {"amount": "5", "precision": 13, "nai": "@@000000099"},
        {"amount": "5", "precision": 2, "nai": "xyz"},
        {"amount": "5", "precision": 2, "nai": "@@00000009"},
        {"amount": "5", "precision": 2, "nai": "@@0000000099"},
        {"amount": "5", "precision": 2, "nai": "@@000000099\n"},
    ],
)
def test_invalid_json_of_assets_is_rejected(value: str | dict[str, object]) -> None:
    # ACT & ASSERT
    with pytest.raises(ValueError, match="asset"):
        Asset.from_builtins(value)


@pytest.mark.parametrize("create", [Asset, Asset.from_builtins])
@pytest.mark.parametrize("value", [1001, None, ["1.001 HIVE"]])
def test_asset_cannot_be_built_from_other_json_types(create: Callable[[object], Asset], value: object) -> None:
    # ACT & ASSERT
    with pytest.raises(TypeError, match="string or an object"):
        create(value)


@pytest.mark.parametrize(
    "create",
    [
        lambda: Asset.hive.tokens(1.0001),
        lambda: Asset.vests.from_str("1.001 HIVE"),
        lambda: Asset.hive.factory(amount=1, nai="@@000000013"),
        lambda: Asset.hive.factory(amount=1, precision=6),
    ],
)
def test_kinds_reject_values_of_other_kinds(create: object) -> None:
    # ACT & ASSERT
    with pytest.raises(ValueError):
        create()  # type: ignore[operator]


@pytest.mark.parametrize("amount", ["abc", "1e999999", float("inf"), float("nan"), Decimal("-Infinity")])
def test_amount_in_tokens_has_to_be_a_finite_number(amount: object) -> None:
    # ACT & ASSERT
    with pytest.raises(ValueError, match="is not an amount of HIVE"):
        Asset.hive.tokens(amount)  # type: ignore[arg-type]


@pytest.mark.parametrize(
    "create",
    [
        lambda: Asset.hive.tokens(None),  # type: ignore[arg-type]
        lambda: Asset.hive.tokens([1]),  # type: ignore[arg-type]
        lambda: Asset.from_nai(5),  # type: ignore[arg-type]
        lambda: Asset.from_str(5),  # type: ignore[arg-type]
    ],
)
def test_values_of_wrong_types_are_rejected(create: object) -> None:
    # ACT & ASSERT
    with pytest.raises(TypeError):
        create()  # type: ignore[operator]


@pytest.mark.parametrize("amount", [True, 1.0, "1"])
def test_satoshi_has_to_be_an_integer(amount: object) -> None:
    # ACT & ASSERT
    with pytest.raises(TypeError, match="has to be an int"):
        Asset.hive.satoshi(amount)  # type: ignore[arg-type]


def test_assets_of_other_kinds_have_no_legacy_form() -> None:
    # ARRANGE
    asset = Asset({"amount": "7", "precision": 2, "nai": "@@000000099"})

    # ACT & ASSERT
    assert asset.symbol == "@@000000099"
    with pytest.raises(ValueError, match="no legacy form"):
        asset.as_legacy()


def test_asset_is_immutable() -> None:
    # ARRANGE
    asset = Asset.hive.satoshi(1)

    # ACT & ASSERT
    with pytest.raises(AttributeError, match="immutable"):
        asset._satoshi = 2
    assert asset.amount == 1


@pytest.mark.parametrize(
    "asset",
    [
        Asset.hive.satoshi(1001),
        Asset.hbd.satoshi(1001, testnet=True),
        Asset("-1.001 TESTS"),
        Asset({"amount": "7", "precision": 2, "nai": "@@000000099"}),
    ],
)
def test_copies_are_the_same_asset_in_the_same_form(asset: Asset) -> None:
    # ACT
    copies = [copy.copy(asset), copy.deepcopy(asset), pickle.loads(pickle.dumps(asset))]

    # ASSERT
    for copied in copies:
        assert type(copied) is Asset
        assert copied == asset
        assert copied.to_builtins() == asset.to_builtins()
        assert str(copied) == str(asset)


@pytest.mark.parametrize(
    ("asset", "expected"),
    [
        (Asset.hive.satoshi(1001), b'[{"amount":"1001","precision":3,"nai":"@@000000021"}]'),
        (Asset("1.001 TESTS"), b'["1.001 TESTS"]'),
    ],
)
def test_assets_are_encoded_in_their_form(asset: Asset, expected: bytes) -> None:
    # ACT & ASSERT
    assert get_hf26_encoder().encode([asset]) == expected


def test_models_with_assets_are_converted_to_dicts() -> None:
    # ARRANGE
    @dataclass
    class Account:
        balance: Asset

    # ACT
    as_dict = asdict(Account(balance=Asset("1.001 TESTS")))

    # ASSERT
    assert as_dict == {"balance": Asset("1.001 TESTS")}
    assert as_dict["balance"].to_builtins() == "1.001 TESTS"


@pytest.mark.parametrize(
    ("asset", "expected"),
    [
        (Asset.hive.satoshi(1001), "Asset.hive.satoshi(1001)"),
        (Asset.hbd.satoshi(1001, testnet=True), "Asset.hbd.satoshi(1001, testnet=True)"),
        (Asset("1.001 TBD"), "Asset('1.001 TBD')"),
        (
            Asset({"amount": "7", "precision": 2, "nai": "@@000000099"}),
            "Asset({'amount': '7', 'precision': 2, 'nai': '@@000000099'})",
        ),
    ],
)
def test_repr_creates_the_same_asset(asset: Asset, expected: str) -> None:
    # ACT & ASSERT
    assert repr(asset) == expected
    assert eval(expected) == asset
