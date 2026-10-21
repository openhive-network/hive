from __future__ import annotations

import json
import warnings
from dataclasses import dataclass
from typing import TYPE_CHECKING, Final

import pytest

import schemas
from tests.generated_package import PACKAGE_DIR, require_generated_package
from tests.kinds_of_assets import literals_of_kinds_of_assets

require_generated_package()

import hiveio_api  # noqa: E402
from hiveio_api import Asset, InvalidValueWarning  # noqa: E402
from hiveio_api._apis.abc.sendable import AsyncSendable  # noqa: E402
from hiveio_api._base import HiveModel, to_api_builtins, to_builtins  # noqa: E402
from hiveio_api.condenser_api import CondenserApi  # noqa: E402
from hiveio_api.database_api import DatabaseApi  # noqa: E402
from hiveio_api.operations import (  # noqa: E402
    Hf26TransferOperation,
    Transaction,
    TransactionLegacy,
    TransferOperation,
    TransferOperationLegacy,
)

# names of the aliases tell the form of assets - they have to be available at runtime (postponed annotations)
from schemas.fields.public import AnyAsset, AssetHive, LegacyAnyAsset, LegacyAssetHbd  # noqa: E402,TCH001

if TYPE_CHECKING:
    from hiveio_api._communication.jsonrpc import ExpectResultT, JSONRPCResult


@dataclass(kw_only=True)
class Payouts(HiveModel):
    nai: AssetHive
    legacy: LegacyAssetHbd
    optional: AnyAsset | None = None
    legacy_list: list[LegacyAnyAsset] | None = None


DATA: Final[dict[str, object]] = {
    "nai": {"amount": "1001", "precision": 3, "nai": "@@000000021"},
    "legacy": "2.000 TBD",
    "optional": {"amount": "3", "precision": 6, "nai": "@@000000037"},
    "legacy_list": ["1.000 HIVE", "0.500 HBD"],
}


def test_assets_of_both_forms_are_built_as_asset() -> None:
    # ACT
    with warnings.catch_warnings():
        warnings.simplefilter("error")
        payouts = Payouts.from_builtins(DATA)

    # ASSERT
    assert payouts.nai == Asset.hive.satoshi(1001)
    assert payouts.legacy == Asset.hbd.satoshi(2000)
    assert payouts.optional == Asset.vests.satoshi(3)
    assert payouts.legacy_list == [Asset.hive.tokens(1), Asset.hbd.tokens("0.5")]
    assert [payouts.nai.is_legacy, payouts.legacy.is_legacy] == [False, True]


def test_assets_are_serialized_back_to_the_form_they_came_in() -> None:
    # ARRANGE
    payouts = Payouts.from_builtins(DATA)

    # ACT & ASSERT
    assert payouts.to_builtins() == DATA
    assert json.loads(payouts.json()) == DATA


def test_assets_take_the_form_of_their_fields_in_builtins_and_in_requests() -> None:
    # ARRANGE - assets created in the other form than their fields take
    payouts = Payouts(nai=Asset("1.001 HIVE"), legacy=Asset.hbd.satoshi(2000), legacy_list=[Asset.hive.satoshi(1000)])

    # ACT
    builtins = payouts.to_builtins()
    request = to_api_builtins(payouts, testnet=lambda: True)

    # ASSERT - the same forms, only symbols of legacy assets follow the network of the receiver
    assert builtins == {"nai": DATA["nai"], "legacy": "2.000 HBD", "legacy_list": ["1.000 HIVE"]}
    assert request == {"nai": DATA["nai"], "legacy": "2.000 TBD", "legacy_list": ["1.000 TESTS"]}
    assert builtins == to_builtins(payouts)


def test_models_are_converted_to_builtins_by_the_public_function() -> None:
    # ARRANGE
    import hiveio_api

    payouts = Payouts.from_builtins(DATA)
    transfer = TransferOperation(from_="alice", to="bob", amount=Asset("1.000 HIVE"), memo="")

    # ACT
    converted = hiveio_api.to_builtins([payouts, {"transfer": transfer}])

    # ASSERT - assets take the form of their fields (`amount` of the transfer takes NAI assets)
    nai_amount = {"amount": "1000", "precision": 3, "nai": "@@000000021"}
    assert converted == [DATA, {"transfer": {"from": "alice", "to": "bob", "amount": nai_amount, "memo": ""}}]


def test_missing_optional_assets_are_none() -> None:
    # ACT
    payouts = Payouts.from_builtins({"nai": DATA["nai"], "legacy": DATA["legacy"]})

    # ASSERT
    assert (payouts.optional, payouts.legacy_list) == (None, None)


def test_value_which_is_not_an_asset_is_kept_with_a_warning() -> None:
    # ACT
    with pytest.warns(InvalidValueWarning, match="`'2.0 TBD'` cannot be built as Asset, kept as it is"):
        payouts = Payouts.from_builtins({**DATA, "legacy": "2.0 TBD"})

    # ASSERT
    assert payouts.legacy == "2.0 TBD"
    assert payouts.to_builtins()["legacy"] == "2.0 TBD"


def test_warning_of_value_which_is_not_an_asset_can_be_disabled() -> None:
    # ACT
    with warnings.catch_warnings():
        warnings.simplefilter("error")
        payouts = Payouts.from_builtins({**DATA, "nai": {"amount": "x"}}, warn=False)

    # ASSERT
    assert payouts.nai == {"amount": "x"}


@pytest.mark.parametrize(
    ("options", "expected"),
    [
        ({}, json.dumps(DATA)),
        ({"indent": 2}, json.dumps(DATA, indent=2)),
        ({"separators": (",", ":")}, json.dumps(DATA, separators=(",", ":"))),
        ({"order": "sorted"}, json.dumps(DATA, sort_keys=True)),
        ({"order": "deterministic"}, json.dumps(DATA, sort_keys=True)),
    ],
)
def test_json_takes_options_of_json_dumps(options: dict[str, object], expected: str) -> None:
    # ARRANGE
    payouts = Payouts.from_builtins(DATA)

    # ACT & ASSERT
    assert payouts.json(**options) == expected  # type: ignore[arg-type]


def test_json_keeps_non_ascii_characters_unless_asked_to_escape_them() -> None:
    """Models of schemas gave UTF-8 JSON - so do public models."""
    # ARRANGE
    memo = "Z\N{LATIN SMALL LETTER U WITH DIAERESIS}rich"
    transfer = TransferOperation(from_="alice", to="bob", amount=Asset.hive.satoshi(1), memo=memo)

    # ACT & ASSERT
    assert f'"memo": "{memo}"' in transfer.json()
    assert '"memo": "Z\\u00fcrich"' in transfer.json(ensure_ascii=True)


def test_copy_is_deep_and_sets_excluded_fields_to_none() -> None:
    # ARRANGE
    transfer = TransferOperation(from_="alice", to="bob", amount=Asset.hive.satoshi(1), memo="memo")

    # ACT
    copied = transfer.copy(exclude={"memo"})

    # ASSERT
    assert copied is not transfer
    assert (copied.from_, copied.to, copied.amount, copied.memo) == ("alice", "bob", Asset.hive.satoshi(1), None)
    assert transfer.memo == "memo"
    assert transfer.copy() == transfer


def test_assets_in_parameters_take_the_form_of_their_fields() -> None:
    # ARRANGE
    payouts = Payouts(
        nai=Asset("1.001 HIVE"),
        legacy=Asset.hbd.satoshi(2000),
        optional=None,
        legacy_list=[Asset({"amount": "1000", "precision": 3, "nai": "@@000000021"})],
    )

    # ACT
    on_testnet = to_api_builtins(payouts, testnet=lambda: True)
    on_mainnet = to_api_builtins(payouts, testnet=lambda: False)

    # ASSERT
    assert on_testnet == {
        "nai": {"amount": "1001", "precision": 3, "nai": "@@000000021"},
        "legacy": "2.000 TBD",
        "legacy_list": ["1.000 TESTS"],
    }
    assert on_mainnet == {**on_testnet, "legacy": "2.000 HBD", "legacy_list": ["1.000 HIVE"]}


def test_assets_outside_fields_of_assets_keep_their_form_with_symbols_of_the_network() -> None:
    # ACT
    params = to_api_builtins(
        [Asset.hive.satoshi(1), Asset("1.000 HIVE"), {"x": Asset("1.000 TBD")}], testnet=lambda: False
    )

    # ASSERT
    assert params == [{"amount": "1", "precision": 3, "nai": "@@000000021"}, "1.000 HIVE", {"x": "1.000 HBD"}]


def test_network_is_asked_only_for_assets_sent_in_the_legacy_form() -> None:
    # ARRANGE
    asked: list[bool] = []

    def testnet() -> bool:
        asked.append(True)
        return True

    # ACT
    to_api_builtins({"payouts": Payouts(nai=Asset("1.000 HIVE"), legacy=None)}, testnet=testnet)  # type: ignore[arg-type]
    asked_for_nai = len(asked)
    to_api_builtins(Payouts(nai=Asset.hive.satoshi(1), legacy=Asset.hbd.satoshi(1)), testnet=testnet)

    # ASSERT
    assert (asked_for_nai, len(asked)) == (0, 1)


class Owner(AsyncSendable):
    def __init__(self, *, testnet: bool) -> None:
        self.testnet = testnet

    def is_testnet(self) -> bool:
        return self.testnet

    async def _async_send(self, **_: object) -> JSONRPCResult[ExpectResultT]:
        raise NotImplementedError


TRANSACTION_MEMBERS: Final[dict[str, object]] = {
    "ref_block_num": 1,
    "ref_block_prefix": 2,
    "expiration": "2026-01-01T00:00:00",
    "extensions": [],
    "signatures": [],
}


@pytest.mark.parametrize(("testnet", "symbol"), [(True, "TESTS"), (False, "HIVE")])
def test_condenser_api_gets_assets_of_transactions_in_the_legacy_form(*, testnet: bool, symbol: str) -> None:
    # ARRANGE
    transfer = TransferOperationLegacy(from_="alice", to="bob", amount=Asset.hive.satoshi(1), memo="")
    transaction = TransactionLegacy(**TRANSACTION_MEMBERS, operations=[("transfer", transfer)])  # type: ignore[arg-type]

    # ACT
    params = json.loads(CondenserApi(Owner(testnet=testnet)).json_dumps()([transaction]))

    # ASSERT
    assert params[0]["operations"] == [
        ["transfer", {"from": "alice", "to": "bob", "amount": f"0.001 {symbol}", "memo": ""}]
    ]


def test_database_api_gets_assets_of_transactions_in_the_nai_form() -> None:
    # ARRANGE
    transfer = TransferOperation(from_="alice", to="bob", amount=Asset("0.001 TESTS"), memo="")
    transaction = Transaction(**TRANSACTION_MEMBERS, operations=[Hf26TransferOperation(value=transfer)])  # type: ignore[arg-type]

    # ACT
    params = json.loads(DatabaseApi(Owner(testnet=True)).json_dumps()({"trx": transaction}))

    # ASSERT
    assert params["trx"]["operations"] == [
        {
            "type": "transfer_operation",
            "value": {
                "from": "alice",
                "to": "bob",
                "amount": {"amount": "1", "precision": 3, "nai": "@@000000021"},
                "memo": "",
            },
        }
    ]


def test_generated_models_of_operations_take_kinds_of_assets_from_schemas() -> None:
    # ARRANGE
    modules = sorted((PACKAGE_DIR / "operations").glob("*.py"))

    # ACT
    literals = [literal for module in modules for literal in literals_of_kinds_of_assets(module)]

    # ASSERT
    assert modules
    assert not literals, "defaults of assets have to refer to kinds of assets (`Asset.hbd`), not to NAIs or symbols"


def test_package_gives_details_of_kinds_of_assets_of_schemas() -> None:
    # ACT & ASSERT
    assert (hiveio_api.HIVE_ASSET, hiveio_api.HBD_ASSET, hiveio_api.VESTS_ASSET) == (
        schemas.HIVE_ASSET,
        schemas.HBD_ASSET,
        schemas.VESTS_ASSET,
    )
    assert hiveio_api.AssetInfo is schemas.AssetInfo
