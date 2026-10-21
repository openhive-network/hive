from __future__ import annotations

import importlib.util
import json
import sys
import typing
import warnings
from typing import TYPE_CHECKING, Final

import pytest

from tests.generated_package import require_generated_package

require_generated_package()

from api_generation.generate_operations import generate_operations  # noqa: E402
from hiveio_api._base import (  # noqa: E402
    OperationBase,
    UnexpectedFieldWarning,
    VirtualOperationBase,
    to_builtins,
    value_builder,
)

import msgspec  # noqa: E402
from schemas.fields.validation import AssetHbd, LegacyAssetHive  # noqa: E402
from schemas import Asset  # noqa: E402
from schemas.validation import validate  # noqa: E402

if TYPE_CHECKING:
    from pathlib import Path
    from types import ModuleType

ACCOUNT: Final[dict[str, str]] = {"type": "string", "x-hive-type": "AccountName"}
DESCRIPTION: Final[dict[str, object]] = {
    "operations": [
        {
            "name": "transfer",
            "id": 2,
            "virtual": False,
            "value": {
                "type": "object",
                "properties": {
                    "from": ACCOUNT,
                    "to": ACCOUNT,
                    "amount": {"x-hive-type": "asset", "x-hive-asset-symbols": ["HIVE", "HBD"]},
                    "memo": {"type": "string"},
                },
                "additionalProperties": False,
                "required": ["from", "to", "amount", "memo"],
            },
        },
        {
            "name": "comment_options",
            "id": 19,
            "virtual": False,
            "value": {
                "type": "object",
                "properties": {
                    "author": ACCOUNT,
                    "max_accepted_payout": {
                        "x-hive-type": "asset",
                        "x-hive-asset-symbols": ["HBD"],
                        "default": {"amount": 1000000000, "symbol": "HBD"},
                    },
                    "allow_votes": {"type": "boolean", "default": True},
                    "extensions": {
                        "type": "array",
                        "items": {"x-hive-variant": [{"$ref": "#/types/comment_payout_beneficiaries"}]},
                    },
                },
                "additionalProperties": False,
                "required": ["author", "max_accepted_payout", "allow_votes", "extensions"],
            },
        },
        {
            "name": "custom_json",
            "id": 18,
            "virtual": False,
            "value": {
                "type": "object",
                "properties": {
                    "id": {"type": "string", "x-hive-type": "CustomIdType"},
                    "json": {"type": "string", "x-hive-type": "JsonString"},
                    "memo_key": {"type": "string", "x-hive-type": "PublicKey"},
                    "extensions": {"type": "array", "items": {"x-hive-variant": [None]}},
                },
                "additionalProperties": False,
                "required": ["id", "json", "extensions"],
            },
        },
        {
            "name": "producer_reward",
            "id": 64,
            "virtual": True,
            "value": {
                "type": "object",
                "properties": {
                    "producer": ACCOUNT,
                    "vesting_shares": {"x-hive-type": "asset", "x-hive-asset-symbols": ["HIVE", "VESTS"]},
                },
                "additionalProperties": False,
                "required": ["producer", "vesting_shares"],
            },
        },
    ],
    "types": {
        "comment_payout_beneficiaries": {
            "type": "object",
            "properties": {"beneficiaries": {"type": "array", "items": {"$ref": "#/types/beneficiary_route_type"}}},
            "additionalProperties": False,
            "required": ["beneficiaries"],
        },
        "beneficiary_route_type": {
            "type": "object",
            "properties": {"account": ACCOUNT, "weight": {"type": "integer", "x-hive-type": "Uint16t"}},
            "additionalProperties": False,
            "required": ["account", "weight"],
        },
    },
}

OPENAPI_SCHEMAS: Final[dict[str, object]] = {
    "transaction_template": {
        "type": "object",
        "properties": {
            "ref_block_num": {"x-hive-type": "Uint16t", "type": "integer"},
            "ref_block_prefix": {"x-hive-type": "Uint32t", "type": "integer"},
            "expiration": {"type": "string", "format": "date-time"},
            "extensions": {"$ref": "#/components/schemas/extensions"},
            "signatures": {"type": "array", "items": {"x-hive-type": "Signature", "type": "string"}},
        },
        "required": ["ref_block_num", "ref_block_prefix", "expiration", "extensions", "signatures"],
    },
    "extensions": {"type": "array", "items": {"type": "object", "properties": {"type": {"type": "string"}}}},
}

NAI_HIVE: Final[dict[str, object]] = {"amount": "1000", "precision": 3, "nai": "@@000000021"}
TRANSFER: Final[dict[str, object]] = {"from": "alice", "to": "bob", "amount": NAI_HIVE, "memo": "m"}
HF26_TRANSFER: Final[dict[str, object]] = {"type": "transfer_operation", "value": TRANSFER}
LEGACY_TRANSFER: Final[list[object]] = ["transfer", {**TRANSFER, "amount": "1.000 HIVE"}]
BENEFICIARIES: Final[dict[str, object]] = {"beneficiaries": [{"account": "bob", "weight": 100}]}
MAX_ACCEPTED_PAYOUT: Final[dict[str, object]] = {"amount": "1000000000", "precision": 3, "nai": "@@000000013"}
HF26_COMMENT_OPTIONS: Final[dict[str, object]] = {
    "type": "comment_options_operation",
    "value": {
        "author": "alice",
        "max_accepted_payout": MAX_ACCEPTED_PAYOUT,
        "allow_votes": True,
        "extensions": [{"type": "comment_payout_beneficiaries", "value": BENEFICIARIES}],
    },
}
LEGACY_COMMENT_OPTIONS_VALUE: Final[dict[str, object]] = {
    "author": "alice",
    "max_accepted_payout": "1000000.000 HBD",
    "allow_votes": True,
    "extensions": [[0, BENEFICIARIES]],
}
LEGACY_COMMENT_OPTIONS: Final[list[object]] = ["comment_options", LEGACY_COMMENT_OPTIONS_VALUE]
SIGNATURE: Final[str] = "1f" + "00" * 64
TRANSACTION_TEMPLATE: Final[dict[str, object]] = {
    "ref_block_num": 1,
    "ref_block_prefix": 2,
    "expiration": "2026-01-01T00:00:00",
    "extensions": [],
    "signatures": [SIGNATURE],
}
HF26_TRANSACTION: Final[dict[str, object]] = {
    **TRANSACTION_TEMPLATE,
    "operations": [HF26_TRANSFER, HF26_COMMENT_OPTIONS],
}
LEGACY_TRANSACTION: Final[dict[str, object]] = {
    **TRANSACTION_TEMPLATE,
    "operations": [LEGACY_TRANSFER, LEGACY_COMMENT_OPTIONS],
}


@pytest.fixture(scope="module")
def modules(tmp_path_factory: pytest.TempPathFactory) -> tuple[ModuleType, ModuleType]:
    base_directory = tmp_path_factory.mktemp("generated")
    description = base_directory / "operations_description.json"
    description.write_text(json.dumps(DESCRIPTION))
    openapi = base_directory / "openapi.json"
    openapi.write_text(json.dumps({"components": {"schemas": OPENAPI_SCHEMAS}}))
    public_path, validation_path = generate_operations(description, openapi, base_directory)
    return load(public_path, "generated_public_operations"), load(validation_path, "generated_validation_operations")


def load(path: Path, name: str) -> ModuleType:
    """Load a generated module - or a package (`__init__.py`), its modules are imported as `<name>.<module>`."""
    locations = [str(path.parent)] if path.name == "__init__.py" else None
    spec = importlib.util.spec_from_file_location(name, path, submodule_search_locations=locations)
    assert spec is not None
    assert spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module  # dataclasses resolve ClassVar annotations in the module
    spec.loader.exec_module(module)
    return module


@pytest.mark.parametrize("operation", [HF26_TRANSFER, HF26_COMMENT_OPTIONS])
def test_hf26_operation_is_built_as_its_public_model(
    modules: tuple[ModuleType, ModuleType], operation: dict[str, object]
) -> None:
    # ARRANGE
    public, _ = modules

    # ACT
    with warnings.catch_warnings():
        warnings.simplefilter("error", UnexpectedFieldWarning)
        model = value_builder(public.Hf26Operation)(operation)

    # ASSERT
    assert model.type == operation["type"]
    assert model.to_builtins() == operation


def test_typed_access_to_public_operations(modules: tuple[ModuleType, ModuleType]) -> None:
    # ARRANGE
    public, _ = modules

    # ACT
    hf26 = value_builder(public.Hf26Operation)(HF26_TRANSFER)
    legacy = value_builder(public.LegacyOperation)(LEGACY_COMMENT_OPTIONS)

    # ASSERT
    assert isinstance(hf26.value, public.TransferOperation)
    assert hf26.value.from_ == "alice"
    assert isinstance(legacy, tuple)
    assert isinstance(legacy[1], public.CommentOptionsOperationLegacy)
    assert isinstance(legacy[1].extensions[0][1], public.CommentPayoutBeneficiaries)
    assert legacy[1].extensions[0][1].beneficiaries[0].account == "bob"


@pytest.mark.parametrize("operation", [LEGACY_TRANSFER, LEGACY_COMMENT_OPTIONS])
def test_legacy_operation_is_built_as_tuple(modules: tuple[ModuleType, ModuleType], operation: list[object]) -> None:
    # ARRANGE
    public, _ = modules

    # ACT
    model = value_builder(public.LegacyOperation)(operation)

    # ASSERT
    assert isinstance(model, tuple)
    assert to_builtins(model) == operation


@pytest.mark.parametrize(
    ("union", "operation"),
    [
        ("Hf26Operation", HF26_TRANSFER),
        ("Hf26Operation", HF26_COMMENT_OPTIONS),
        ("LegacyOperation", LEGACY_TRANSFER),
        ("LegacyOperation", LEGACY_COMMENT_OPTIONS),
    ],
)
def test_operations_are_valid_in_their_form(
    modules: tuple[ModuleType, ModuleType], union: str, operation: object
) -> None:
    # ARRANGE
    _, validation = modules

    # ACT & ASSERT
    assert validate(operation, getattr(validation, union)) == []


@pytest.mark.parametrize(
    ("union", "operation", "path"),
    [
        ("Hf26Operation", {"type": "transfer_operation", "value": LEGACY_TRANSFER[1]}, "$.value.amount"),
        ("LegacyOperation", ["transfer", TRANSFER], "$[1].amount"),
        ("Hf26Operation", {"type": "transfer_operation", "value": {**TRANSFER, "to": "Invalid!"}}, "$.value.to"),
        ("Hf26Operation", {"type": "unknown_operation", "value": {}}, "$.type"),
        (
            "LegacyOperation",
            ["comment_options", {**LEGACY_COMMENT_OPTIONS_VALUE, "extensions": [[1, {}]]}],
            "$[1].extensions[0][0]",
        ),
    ],
)
def test_operations_are_invalid_in_other_form(
    modules: tuple[ModuleType, ModuleType], union: str, operation: object, path: str
) -> None:
    # ARRANGE
    _, validation = modules

    # ACT
    errors = validate(operation, getattr(validation, union))

    # ASSERT
    assert [error.path for error in errors] == [path], errors


def test_defaults_in_both_forms(modules: tuple[ModuleType, ModuleType]) -> None:
    # ARRANGE
    public, validation = modules

    # ACT
    public_hf26 = public.CommentOptionsOperation(author="alice", extensions=[])
    public_legacy = public.CommentOptionsOperationLegacy(author="alice", extensions=[])
    validation_hf26 = validation.CommentOptionsOperation(author="alice", extensions=[])
    validation_legacy = validation.CommentOptionsOperationLegacy(author="alice", extensions=[])

    # ASSERT
    assert public_hf26.to_builtins()["max_accepted_payout"] == MAX_ACCEPTED_PAYOUT
    assert public_legacy.max_accepted_payout == Asset.hbd.satoshi(1000000000)
    assert public_legacy.to_builtins()["max_accepted_payout"] == "1000000.000 HBD"
    assert public_hf26.allow_votes is True
    assert validation_hf26.max_accepted_payout == msgspec.convert(MAX_ACCEPTED_PAYOUT, AssetHbd)
    assert json.loads(validation_legacy.json())["max_accepted_payout"] == "1000000.000 HBD"


def test_operation_metadata(modules: tuple[ModuleType, ModuleType]) -> None:
    # ARRANGE
    public, validation = modules

    # ACT & ASSERT
    for module in (public, validation):
        assert module.TransferOperation.operation_id == 2
        assert module.TransferOperation.virtual is False
        assert module.ProducerRewardOperation.virtual is True


def test_public_values_of_operations_derive_base_classes_of_operations(modules: tuple[ModuleType, ModuleType]) -> None:
    # ARRANGE
    public, _ = modules

    # ACT & ASSERT
    assert public.OperationBase is OperationBase
    assert public.VirtualOperationBase is VirtualOperationBase
    for value in (public.TransferOperation, public.TransferOperationLegacy, public.CommentOptionsOperation):
        assert issubclass(value, OperationBase)
        assert not issubclass(value, VirtualOperationBase)
    assert issubclass(public.ProducerRewardOperation, VirtualOperationBase)
    assert not issubclass(public.CommentPayoutBeneficiaries, OperationBase)


def test_operations_are_named_in_the_notation_of_hived(modules: tuple[ModuleType, ModuleType]) -> None:
    # ARRANGE
    public, validation = modules

    # ACT
    public_operation = value_builder(public.Hf26Operation)(HF26_COMMENT_OPTIONS)
    validation_operation = validation.Hf26CommentOptionsOperation.parse_builtins(HF26_COMMENT_OPTIONS)

    # ASSERT
    for module in (public, validation):
        for value in (module.TransferOperation, module.TransferOperationLegacy):
            assert value.get_name() == "transfer"
            assert value.get_name_with_suffix() == "transfer_operation"
        assert module.Hf26TransferOperation.type == "transfer_operation"
    for operation in (public_operation, validation_operation):
        assert operation.type == "comment_options_operation"
        assert operation.type == operation.value.get_name_with_suffix()
        assert operation.value.extensions[0].type == "comment_payout_beneficiaries"
    assert "type" not in validation.Hf26CommentOptionsOperation.__struct_fields__  # the tag stays the only `type`
    assert json.loads(validation_operation.json())["type"] == "comment_options_operation"


def test_reserved_names_are_renamed_and_optional_members_default_to_none(
    modules: tuple[ModuleType, ModuleType],
) -> None:
    # ARRANGE
    public, validation = modules
    custom_json = {"id": "follow", "json": "[]", "extensions": []}

    # ACT
    public_model = value_builder(public.CustomJsonOperation)(custom_json)
    validation_errors = validate(custom_json, validation.CustomJsonOperation)

    # ASSERT
    assert public_model.id_ == "follow"
    assert public_model.json_ == "[]"
    assert public_model.memo_key is None
    assert validation_errors == []
    assert (
        validate({**custom_json, "extensions": [{"type": "void_t", "value": {}}]}, validation.CustomJsonOperation) != []
    )


def test_forms_are_generated_only_for_structures_with_assets(modules: tuple[ModuleType, ModuleType]) -> None:
    # ARRANGE
    public, validation = modules
    custom_json = {"id": "follow", "json": "[]", "extensions": []}
    build_legacy = value_builder(public.LegacyOperation)

    # ACT
    _, transfer = build_legacy(LEGACY_TRANSFER)
    _, legacy_custom_json = build_legacy(["custom_json", custom_json])
    _, comment_options = build_legacy(LEGACY_COMMENT_OPTIONS)

    # ASSERT - a value without assets has one class of both forms
    assert type(transfer) is public.TransferOperationLegacy
    assert transfer.to_builtins()["amount"] == "1.000 HIVE"
    assert type(legacy_custom_json) is public.CustomJsonOperation
    assert type(comment_options.extensions[0][1]) is public.CommentPayoutBeneficiaries
    assert validate(["custom_json", custom_json], validation.LegacyOperation) == []
    for module in (public, validation):
        assert not hasattr(module, "CustomJsonOperationLegacy")
        assert not hasattr(module, "CommentPayoutBeneficiariesLegacy")


def test_legacy_assets_of_validation_models(modules: tuple[ModuleType, ModuleType]) -> None:
    # ARRANGE
    _, validation = modules

    # ACT
    operation = validation.LegacyOperation.__args__[0].parse_builtins(LEGACY_TRANSFER)  # type: ignore[attr-defined]

    # ASSERT
    assert operation.value.amount == "1.000 HIVE", "assets of the legacy form are strings checked by their pattern"
    assert msgspec.convert(operation.value.amount, LegacyAssetHive) == operation.value.amount


def test_hf26_operation_is_created_without_its_type(modules: tuple[ModuleType, ModuleType]) -> None:
    # ARRANGE
    public, _ = modules

    # ACT
    operation = public.Hf26TransferOperation(
        value=public.TransferOperation(from_="alice", to="bob", amount=Asset.hive.satoshi(1000), memo="m")
    )

    # ASSERT
    assert operation.to_builtins() == HF26_TRANSFER


@pytest.mark.parametrize(
    ("transaction_class", "transaction"),
    [("Transaction", HF26_TRANSACTION), ("TransactionLegacy", LEGACY_TRANSACTION)],
)
def test_transactions_are_built_as_public_models(
    modules: tuple[ModuleType, ModuleType], transaction_class: str, transaction: dict[str, object]
) -> None:
    # ARRANGE
    public, _ = modules

    # ACT
    with warnings.catch_warnings():
        warnings.simplefilter("error", UnexpectedFieldWarning)
        model = getattr(public, transaction_class).from_builtins(transaction)

    # ASSERT
    first_operation = model.operations[0]
    if transaction is HF26_TRANSACTION:
        assert first_operation.type == "transfer_operation"
    else:
        assert isinstance(first_operation, tuple)
        assert first_operation[0] == "transfer"
    assert model.to_builtins() == transaction


@pytest.mark.parametrize(
    ("transaction_class", "transaction", "paths"),
    [
        ("Transaction", HF26_TRANSACTION, []),
        ("TransactionLegacy", LEGACY_TRANSACTION, []),
        ("TransactionLegacy", HF26_TRANSACTION, ["$.operations[0]", "$.operations[1]"]),
        ("Transaction", {**HF26_TRANSACTION, "ref_block_num": 70000}, ["$.ref_block_num"]),
        ("Transaction", {**HF26_TRANSACTION, "signatures": ["not hex"]}, ["$.signatures[0]"]),
    ],
)
def test_transactions_are_validated_in_their_form(
    modules: tuple[ModuleType, ModuleType], transaction_class: str, transaction: dict[str, object], paths: list[str]
) -> None:
    # ARRANGE
    _, validation = modules

    # ACT
    errors = validate(transaction, getattr(validation, transaction_class))

    # ASSERT
    assert [error.path for error in errors] == paths, errors


def test_unions_of_operation_values(modules: tuple[ModuleType, ModuleType]) -> None:
    # ARRANGE
    public, validation = modules

    operations = DESCRIPTION["operations"]
    assert isinstance(operations, list)

    # ACT & ASSERT
    for module in (public, validation):
        hf26_values = typing.get_args(module.Hf26OperationValue)
        legacy_values = typing.get_args(module.LegacyOperationValue)
        assert module.TransferOperation in hf26_values
        assert module.TransferOperationLegacy in legacy_values
        assert module.CustomJsonOperation in hf26_values
        assert module.CustomJsonOperation in legacy_values
        assert len(hf26_values) == len(legacy_values) == len(operations)


def test_extensions_are_empty_when_not_given(modules: tuple[ModuleType, ModuleType]) -> None:
    # ARRANGE
    public, validation = modules

    # ACT
    public_operation = public.CustomJsonOperation(id_="follow", json_="[]")
    built_operation = public.CustomJsonOperation.from_builtins({"id": "follow", "json": "[]"}, warn=False)
    validation_operation = validation.CustomJsonOperation(id_="follow", json_="[]")

    # ASSERT
    assert public_operation.extensions == []
    assert built_operation.extensions == []
    assert validation_operation.extensions == []
    assert public_operation.to_builtins()["extensions"] == []
    assert "extensions" not in built_operation.to_builtins()  # keys of the data it was built from
