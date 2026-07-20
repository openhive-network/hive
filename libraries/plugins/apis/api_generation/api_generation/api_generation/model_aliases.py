from __future__ import annotations

import ast
import copy
from dataclasses import dataclass
from typing import TYPE_CHECKING, Final

from api_client_generator.semantic_model_aliases import SemanticModelAlias, apply_semantic_model_aliases

if TYPE_CHECKING:
    from pathlib import Path


@dataclass(frozen=True)
class ClassInfo:
    name: str
    node: ast.ClassDef


_TRANSACTION_FIELDS: Final = (
    ("ref_block_num", "int", None),
    ("ref_block_prefix", "int", None),
    ("expiration", "str", None),
    ("extensions", "list[Extension]", None),
    ("signatures", "list[str]", None),
    ("operations", "list[Operation]", None),
)

_BLOCK_FIELDS: Final = (
    ("previous", "str", None),
    ("timestamp", "str", None),
    ("witness", "str", None),
    ("transaction_merkle_root", "str", None),
    ("extensions", "list[Extension]", None),
    ("witness_signature", "str", None),
    ("block_id", "str", None),
    ("signing_key", "str", None),
    ("transaction_ids", "list[str]", None),
    ("transactions", "list[Transaction]", None),
)

_NAI_ASSET_FIELDS: Final = (
    ("amount", "str | int", None),
    ("nai", "str", None),
    ("precision", "int", None),
)

_PRICE_PAIR_FIELDS: Final = (
    ("base", "NaiAsset", None),
    ("quote", "NaiAsset", None),
)

_WITNESS_PROPERTIES_FIELDS: Final = (
    ("account_subsidy_budget", "int", None),
    ("account_subsidy_decay", "int", None),
    ("hbd_interest_rate", "int", None),
    ("maximum_block_size", "int", None),
    ("account_creation_fee", "NaiAsset", None),
)

_KNOWN_DUPLICATE_GROUPS_BY_API: Final[dict[str, tuple[SemanticModelAlias, ...]]] = {
    "database_api": (
        SemanticModelAlias("Account", ("Account", "Account1"), emit_compatibility_aliases=False),
        SemanticModelAlias("Witness", ("Witness", "Witness1"), emit_compatibility_aliases=False),
    ),
    "rc_api": (SemanticModelAlias("RcAccount", ("RcAccount", "RcAccount1"), emit_compatibility_aliases=False),),
}


class _AnnotationCanonicalizer(ast.NodeTransformer):
    def __init__(self, replacements: dict[str, str]) -> None:
        self._replacements = replacements

    def visit_Name(self, node: ast.Name) -> ast.Name:  # noqa: N802
        replacement = self._replacements.get(node.id)
        if replacement is None:
            return node
        return ast.copy_location(ast.Name(id=replacement, ctx=node.ctx), node)


def apply_stable_model_aliases(description_file: Path, api: str) -> None:
    """Apply Hive semantic aliases and move safe shared models to hiveio_api.common."""
    source = description_file.read_text(encoding="utf-8")
    tree = ast.parse(source, filename=str(description_file))
    classes = {
        node.name: ClassInfo(node.name, node)
        for node in tree.body
        if isinstance(node, ast.ClassDef) and any(ast.unparse(base) == "Struct" for base in node.bases)
    }

    groups = _build_groups(api.replace("-", "_"), classes)
    if not groups:
        return

    apply_semantic_model_aliases(
        description_file,
        groups,
        common_file=description_file.parents[1] / "common.py",
        common_import="hiveio_api.common",
    )


def _build_groups(api: str, classes: dict[str, ClassInfo]) -> tuple[SemanticModelAlias, ...]:
    replacements: dict[str, str] = {}
    groups: list[SemanticModelAlias] = []

    _add_common_dependency_groups(classes, groups)

    transaction_members = _classes_matching(classes, _TRANSACTION_FIELDS, replacements)
    if len(transaction_members) > 1:
        groups.append(
            SemanticModelAlias(
                "Transaction",
                transaction_members,
                ("BlockTransaction",),
                emit_compatibility_aliases=False,
            )
        )
        replacements.update(dict.fromkeys(transaction_members, "Transaction"))

    block_members = _classes_matching(classes, _BLOCK_FIELDS, replacements)
    if len(block_members) > 1:
        groups.append(SemanticModelAlias("Block", block_members, emit_compatibility_aliases=False))
        replacements.update(dict.fromkeys(block_members, "Block"))

    asset_members = _classes_matching(classes, _NAI_ASSET_FIELDS, replacements)
    if len(asset_members) > 1:
        groups.append(SemanticModelAlias("NaiAsset", asset_members, emit_compatibility_aliases=False))
        replacements.update(dict.fromkeys(asset_members, "NaiAsset"))

    price_members = _classes_matching(classes, _PRICE_PAIR_FIELDS, replacements)
    if len(price_members) > 1:
        price_aliases = ["FeedPriceHistoryItem"]
        if api == "database_api":
            price_aliases.append("GetCurrentPriceFeedResponse")
        groups.append(
            SemanticModelAlias(
                "PricePair",
                price_members,
                tuple(price_aliases),
                emit_compatibility_aliases=False,
            )
        )
        replacements.update(dict.fromkeys(price_members, "PricePair"))

    witness_property_members = _classes_matching(classes, _WITNESS_PROPERTIES_FIELDS, replacements)
    if len(witness_property_members) > 1:
        groups.append(
            SemanticModelAlias(
                "WitnessProperties",
                witness_property_members,
                ("WitnessProps",),
                emit_compatibility_aliases=False,
            )
        )
        replacements.update(dict.fromkeys(witness_property_members, "WitnessProperties"))

    groups.extend(_KNOWN_DUPLICATE_GROUPS_BY_API.get(api, ()))

    return tuple(groups)


def _add_common_dependency_groups(classes: dict[str, ClassInfo], groups: list[SemanticModelAlias]) -> None:
    if "Extension" in classes:
        groups.append(
            SemanticModelAlias("Extension", ("Extension",), minimum_members=1, emit_compatibility_aliases=False)
        )
    if "Operation" in classes:
        groups.append(
            SemanticModelAlias(
                "Operation",
                ("Operation",),
                ("OperationEnvelope",),
                minimum_members=1,
                emit_compatibility_aliases=False,
            )
        )


def _classes_matching(
    classes: dict[str, ClassInfo],
    expected_fields: tuple[tuple[str, str, str | None], ...],
    replacements: dict[str, str],
) -> tuple[str, ...]:
    return tuple(
        name
        for name, class_info in classes.items()
        if _field_signature(class_info.node, replacements) == expected_fields
    )


def _field_signature(class_node: ast.ClassDef, replacements: dict[str, str]) -> tuple[tuple[str, str, str | None], ...]:
    fields: list[tuple[str, str, str | None]] = []
    for statement in class_node.body:
        if not isinstance(statement, ast.AnnAssign):
            continue
        if not isinstance(statement.target, ast.Name):
            continue

        fields.append((
            statement.target.id,
            _canonical_annotation(statement.annotation, replacements),
            ast.unparse(statement.value) if statement.value is not None else None,
        ))
    return tuple(fields)


def _canonical_annotation(annotation: ast.expr, replacements: dict[str, str]) -> str:
    canonicalized = _AnnotationCanonicalizer(replacements).visit(ast.fix_missing_locations(copy.deepcopy(annotation)))
    return ast.unparse(canonicalized)
