from __future__ import annotations

import ast
import importlib.util
import sys
import textwrap
from pathlib import Path

import msgspec
import pytest
from api_generation.model_aliases import apply_stable_model_aliases
from generate_api_definitions import extract_class_names_from_file
from generate_root_package import generate_root_package
from msgspec import UNSET, Struct, UnsetType


def test_block_api_transactions_and_blocks_use_canonical_types(tmp_path: Path) -> None:
    description = _description_path(tmp_path, "block_api")
    description.write_text(
        textwrap.dedent(
            """
            from __future__ import annotations

            from typing import Any, TypeAlias

            from msgspec import UNSET, Struct, UnsetType

            class Extension(Struct):
                type: str
                value: str | dict[str, Any]

            class Operation(Struct):
                type: str
                value: dict[str, Any]

            class Transaction3(Struct):
                ref_block_num: int
                ref_block_prefix: int
                expiration: str
                extensions: list[Extension]
                signatures: list[str]
                operations: list[Operation]

            class Block(Struct):
                previous: str
                timestamp: str
                witness: str
                transaction_merkle_root: str
                extensions: list[Extension]
                witness_signature: str
                block_id: str
                signing_key: str
                transaction_ids: list[str]
                transactions: list[Transaction3]

            class Transaction4(Struct):
                ref_block_num: int
                ref_block_prefix: int
                expiration: str
                extensions: list[Extension]
                signatures: list[str]
                operations: list[Operation]

            class Block1(Struct):
                previous: str
                timestamp: str
                witness: str
                transaction_merkle_root: str
                extensions: list[Extension]
                witness_signature: str
                block_id: str
                signing_key: str
                transaction_ids: list[str]
                transactions: list[Transaction4]

            class GetBlockResponse(Struct):
                block: Block1 | UnsetType = UNSET

            block_api_description = {
                "block_api": {
                    "get_block": {
                        "params": object,
                        "result": GetBlockResponse,
                        "description": "",
                    },
                }
            }
            """
        )
    )

    apply_stable_model_aliases(description, "block_api")

    classes = _classes_from(description)
    common_classes = _classes_from(_common_path(description))
    aliases = _aliases_from(description)
    common_imports = _common_imports_from(description)
    assert "Transaction" not in classes
    assert "Block" not in classes
    assert "Extension" not in classes
    assert "Operation" not in classes
    assert "Transaction" in common_classes
    assert "Block" in common_classes
    assert "Extension" in common_classes
    assert "Operation" in common_classes
    assert {"Block", "Extension", "Operation", "Transaction"} <= common_imports
    assert "Transaction3" not in classes
    assert "Transaction4" not in classes
    assert "Block1" not in classes
    assert aliases["BlockTransaction"] == "Transaction"
    assert aliases["OperationEnvelope"] == "Operation"
    assert "Transaction3" not in aliases
    assert "Transaction4" not in aliases
    assert "Block1" not in aliases
    assert _field_annotation(common_classes["Block"], "transactions") == "list[Transaction]"
    assert _field_annotation(classes["GetBlockResponse"], "block") == "Block | UnsetType"


def test_database_api_exposes_stable_asset_price_and_endpoint_aliases(tmp_path: Path) -> None:
    description = _description_path(tmp_path, "database_api")
    description.write_text(
        textwrap.dedent(
            """
            from __future__ import annotations

            from typing import Any, TypeAlias

            from msgspec import Struct

            class Operation(Struct):
                type: str
                value: dict[str, Any]

            class Balance(Struct):
                amount: str | int
                nai: str
                precision: int

            class HbdBalance(Struct):
                amount: str | int
                nai: str
                precision: int

            class Base(Struct):
                amount: str | int
                nai: str
                precision: int

            class Quote(Struct):
                amount: str | int
                nai: str
                precision: int

            class SellPrice3(Struct):
                base: Base
                quote: Quote

            class CurrentMedianHistory3(Struct):
                base: Base
                quote: Quote

            class Account(Struct):
                name: str

            class Account1(Struct):
                name: str

            class AccountCreationFee(Struct):
                amount: str | int
                nai: str
                precision: int

            class Props3(Struct):
                account_subsidy_budget: int
                account_subsidy_decay: int
                hbd_interest_rate: int
                maximum_block_size: int
                account_creation_fee: AccountCreationFee

            class Props4(Struct):
                account_subsidy_budget: int
                account_subsidy_decay: int
                hbd_interest_rate: int
                maximum_block_size: int
                account_creation_fee: AccountCreationFee

            class Witness(Struct):
                owner: str
                props: Props3

            class Witness1(Struct):
                owner: str
                props: Props4

            database_api_description = {
                "database_api": {
                    "get_current_price_feed": {
                        "params": object,
                        "result": CurrentMedianHistory3,
                        "description": "",
                    },
                }
            }
            """
        )
    )

    apply_stable_model_aliases(description, "database_api")

    classes = _classes_from(description)
    common_classes = _classes_from(_common_path(description))
    aliases = _aliases_from(description)
    common_imports = _common_imports_from(description)
    assert "NaiAsset" not in classes
    assert "PricePair" not in classes
    assert "Account" not in classes
    assert "Witness" not in classes
    assert "WitnessProperties" not in classes
    assert "NaiAsset" in common_classes
    assert "PricePair" in common_classes
    assert "Account" in common_classes
    assert "Witness" in common_classes
    assert "WitnessProperties" in common_classes
    assert {"Account", "NaiAsset", "PricePair", "Witness", "WitnessProperties"} <= common_imports
    assert aliases["FeedPriceHistoryItem"] == "PricePair"
    assert aliases["OperationEnvelope"] == "Operation"
    assert "Balance" not in aliases
    assert "HbdBalance" not in aliases
    assert "Base" not in aliases
    assert "Quote" not in aliases
    assert "SellPrice3" not in aliases
    assert "CurrentMedianHistory3" not in aliases
    assert "Account1" not in aliases
    assert "Props3" not in aliases
    assert "Props4" not in aliases
    assert "Witness1" not in aliases
    assert _field_annotation(common_classes["PricePair"], "base") == "NaiAsset"
    assert _field_annotation(common_classes["Witness"], "props") == "WitnessProperties"


def test_rc_api_drops_generated_rc_account_name(tmp_path: Path) -> None:
    description = _description_path(tmp_path, "rc_api")
    description.write_text(
        textwrap.dedent(
            """
            from __future__ import annotations

            from typing import TypeAlias

            from msgspec import Struct

            class RcAccount(Struct):
                account: str
                rc_manabar: int

            class RcAccount1(Struct):
                account: str
                rc_manabar: int

            rc_api_description = {"rc_api": {}}
            """
        )
    )

    apply_stable_model_aliases(description, "rc_api")

    classes = _classes_from(description)
    common_classes = _classes_from(_common_path(description))
    aliases = _aliases_from(description)
    assert "RcAccount" not in classes
    assert "RcAccount" in common_classes
    assert "RcAccount1" not in classes
    assert "RcAccount1" not in aliases
    assert "RcAccount" in _common_imports_from(description)


def test_optional_field_helper_documents_unset_behavior(tmp_path: Path) -> None:
    package_dir = tmp_path / "python_api_package" / "hiveio_api"
    template_dir = Path(__file__).parents[2] / "python_api_package" / "templates"
    generate_root_package(
        ["block_api"],
        tmp_path,
        template_dir,
    )

    optional_module = _load_module(package_dir / "_optional.py", "generated_optional")
    assert optional_module.optional_field(UNSET) is None
    assert optional_module.optional_field("value") == "value"
    assert '    "optional_field",' not in (package_dir / "__init__.py").read_text()

    class Response(Struct):
        block: str | UnsetType = UNSET

    assert msgspec.json.decode(b"{}", type=Response).block is UNSET
    assert optional_module.optional_field(msgspec.json.decode(b"{}", type=Response).block) is None
    with pytest.raises(msgspec.ValidationError):
        msgspec.json.decode(b'{"block": null}', type=Response)


def test_exported_description_symbols_ignore_model_fields(tmp_path: Path) -> None:
    description = tmp_path / "description.py"
    description.write_text(
        textwrap.dedent(
            """
            from __future__ import annotations

            from typing import TypeAlias

            from msgspec import Struct

            from hiveio_api.common import ImportedModel

            class PublicModel(Struct):
                field_name: str

            PublicAlias: TypeAlias = PublicModel
            """
        )
    )

    assert extract_class_names_from_file(description) == ["ImportedModel", "PublicModel", "PublicAlias"]


def _description_path(tmp_path: Path, api: str) -> Path:
    description = tmp_path / "hiveio_api" / api / f"{api}_description.py"
    description.parent.mkdir(parents=True)
    return description


def _common_path(description: Path) -> Path:
    return description.parents[1] / "common.py"


def _classes_from(path: Path) -> dict[str, ast.ClassDef]:
    return {node.name: node for node in ast.parse(path.read_text()).body if isinstance(node, ast.ClassDef)}


def _aliases_from(path: Path) -> dict[str, str]:
    aliases: dict[str, str] = {}
    for node in ast.parse(path.read_text()).body:
        if not isinstance(node, ast.AnnAssign):
            continue
        if not isinstance(node.target, ast.Name):
            continue
        if ast.unparse(node.annotation) != "TypeAlias":
            continue
        aliases[node.target.id] = ast.unparse(node.value)
    return aliases


def _common_imports_from(path: Path) -> set[str]:
    imports: set[str] = set()
    for node in ast.parse(path.read_text()).body:
        if not isinstance(node, ast.ImportFrom):
            continue
        if node.module != "hiveio_api.common":
            continue
        imports.update(alias.asname or alias.name for alias in node.names)
    return imports


def _field_annotation(class_node: ast.ClassDef, field_name: str) -> str:
    for statement in class_node.body:
        if not isinstance(statement, ast.AnnAssign):
            continue
        if isinstance(statement.target, ast.Name) and statement.target.id == field_name:
            return ast.unparse(statement.annotation)
    raise AssertionError(f"Field {field_name!r} not found in class {class_node.name!r}")


def _load_module(path: Path, module_name: str):
    spec = importlib.util.spec_from_file_location(module_name, path)
    assert spec is not None
    assert spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    sys.modules[module_name] = module
    spec.loader.exec_module(module)
    return module
