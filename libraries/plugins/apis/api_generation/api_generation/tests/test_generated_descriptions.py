from __future__ import annotations

import ast
import json
import textwrap
from pathlib import Path
from typing import Final

import pytest

from tests.generated_package import require_generated_package

require_generated_package()  # the generator imports the field types of hiveio_api

from api_generation.generate_description import strip_default_suffixes  # noqa: E402
from api_generation.model_methods import add_model_methods  # noqa: E402
from api_generation.undeclared_assets import UndeclaredAssetError, check_no_undeclared_assets  # noqa: E402
from api_generation.undeclared_dates import (  # noqa: E402
    UndeclaredDateError,
    check_no_undeclared_dates,
    find_undeclared_dates,
)
from generate_root_package import generate_root_package  # noqa: E402


def write_description(tmp_path: Path, api: str, models: str) -> Path:
    description = tmp_path / "hiveio_api" / api / f"{api}_description.py"
    description.parent.mkdir(parents=True)
    header = "\n".join([
        "from __future__ import annotations",
        "",
        "from dataclasses import dataclass",
        "",
        "from hiveio_api._base import HiveModel",
        "",
    ])
    description.write_text(header + models)
    return description


MANABAR = """
@dataclass(kw_only=True)
class VotingManabar(HiveModel):
    current_mana: int
    last_update_time: int
"""


@pytest.mark.parametrize("variant", ["public", "validation"])
def test_generated_model_of_an_asset_is_an_error(tmp_path: Path, variant: str) -> None:
    # ARRANGE
    base = "HiveModel" if variant == "public" else "Struct"
    description = tmp_path / "database_api_description.py"
    description.write_text(
        textwrap.dedent(
            f"""
            from __future__ import annotations

            class Balance({base}):
                amount: str | int
                nai: str
                precision: int

            class Order({base}):
                price: int
                amount: Balance
            """
        )
    )

    # ACT & ASSERT
    with pytest.raises(UndeclaredAssetError, match=r"`database_api`: Balance - fields `amount`, `nai`, `precision`"):
        check_no_undeclared_assets(description, "database_api")


def test_models_without_fields_of_assets_are_accepted(tmp_path: Path) -> None:
    # ARRANGE
    description = write_description(tmp_path, "database_api", MANABAR)

    # ACT & ASSERT
    check_no_undeclared_assets(description, "database_api")


def definition_with_response(schema: dict[str, object], example: object) -> dict[str, object]:
    """openapi.json with one endpoint of `database_api` - its response described by the schema and the example."""
    response = {
        "content": {"application/json": {"schema": {"$ref": "#/components/schemas/response"}, "example": example}}
    }
    return {
        "paths": {"database_api.get_votes": {"post": {"responses": {"200": response}}}},
        "components": {"schemas": {"response": schema, "date": {"type": "string", "format": "date-time"}}},
    }


VOTES: Final[dict[str, object]] = {
    "type": "object",
    "properties": {
        "voter": {"type": "string"},
        "time": {"$ref": "#/components/schemas/date"},
        "history": {"type": "array", "items": {"anyOf": [{"type": "null"}, {"type": "string", "format": "date-time"}]}},
        "last": {"type": "array", "prefixItems": [{"type": "string"}, {"type": "string", "format": "date-time"}]},
    },
}


def test_dates_declared_in_openapi_json_are_accepted() -> None:
    # ARRANGE
    example = {
        "voter": "alice",
        "time": "2016-03-24T16:05:00",
        "history": [None, "1970-01-01T00:00:00"],
        "last": ["alice", "2016-03-24T16:05:00"],
    }

    # ACT & ASSERT
    assert find_undeclared_dates(definition_with_response(VOTES, example), "database_api") == []


def test_dates_in_strings_without_format_of_dates_are_found() -> None:
    # ARRANGE
    example = [{"voter": "2016-03-24T16:05:00", "last": ["2016-03-24T16:05:00", "x"], "extra": "1970-01-01T00:00:00"}]
    schema = {"type": "array", "items": {"allOf": [VOTES]}}

    # ACT
    undeclared = find_undeclared_dates(definition_with_response(schema, example), "database_api")

    # ASSERT
    assert undeclared == [
        "database_api.get_votes: result[].extra",
        "database_api.get_votes: result[].last[]",
        "database_api.get_votes: result[].voter",
    ]


def test_dates_are_checked_only_in_endpoints_of_the_api() -> None:
    # ARRANGE
    definition = definition_with_response({"type": "string"}, "2016-03-24T16:05:00")

    # ACT & ASSERT
    assert find_undeclared_dates(definition, "condenser_api") == []
    assert find_undeclared_dates(definition, "database_api") == ["database_api.get_votes: result"]


def test_undeclared_date_fails_the_generation(tmp_path: Path) -> None:
    # ARRANGE
    openapi_json = tmp_path / "openapi.json"
    openapi_json.write_text(json.dumps(definition_with_response({"type": "string"}, "2016-03-24T16:05:00")))

    # ACT & ASSERT
    with pytest.raises(UndeclaredDateError, match=r"`database_api`: database_api.get_votes: result - fix openapi.json"):
        check_no_undeclared_dates(openapi_json, ["condenser_api", "database_api"])


def test_default_suffixes_of_components_and_references_to_them_are_stripped() -> None:
    # ARRANGE
    definition = {
        "components": {
            "schemas": {
                "witness_default": {"type": "object", "properties": {"owner": {"type": "string"}}},
                "witness_condenser_api": {"type": "object", "properties": {"owner": {"type": "string"}}},
                "response": {"type": "array", "items": {"$ref": "#/components/schemas/witness_default"}},
            }
        }
    }

    # ACT
    result = json.loads(strip_default_suffixes(json.dumps(definition)))

    # ASSERT
    assert list(result["components"]["schemas"]) == ["witness", "witness_condenser_api", "response"]
    assert result["components"]["schemas"]["response"]["items"] == {"$ref": "#/components/schemas/witness"}


def test_default_suffix_of_a_component_clashing_with_another_is_an_error() -> None:
    # ARRANGE
    definition = {"components": {"schemas": {"witness_default": {"type": "object"}, "witness": {"type": "object"}}}}

    # ACT & ASSERT
    with pytest.raises(ValueError, match=r"\['witness'\] of openapi.json clash"):
        strip_default_suffixes(json.dumps(definition))


def test_hand_written_methods_are_assigned_in_bodies_of_their_models(tmp_path: Path) -> None:
    # ARRANGE
    description = write_description(tmp_path, "database_api", MANABAR + "\n\nclass Other(HiveModel):\n    pass\n")

    # ACT
    add_model_methods([description], {"VotingManabar": {"__eq__": "hiveio_api._model_methods.status_item_eq"}})

    # ASSERT
    module = ast.parse(description.read_text())
    manabar, other = (node for node in module.body if isinstance(node, ast.ClassDef))
    assert [ast.unparse(node) for node in manabar.body if isinstance(node, ast.Assign)] == ["__eq__ = status_item_eq"]
    assert len(other.body) == 1
    imports = [ast.unparse(node) for node in module.body if isinstance(node, ast.ImportFrom)]
    assert "from hiveio_api._model_methods import status_item_eq" in imports


def test_hand_written_methods_of_models_which_are_not_generated_are_an_error(tmp_path: Path) -> None:
    # ARRANGE
    description = write_description(tmp_path, "database_api", MANABAR)

    # ACT & ASSERT
    with pytest.raises(ValueError, match=r"Models \['Missing'\] with hand-written methods are not generated"):
        add_model_methods([description], {"Missing": {"__eq__": "hiveio_api._model_methods.status_item_eq"}})


def test_root_package_lists_apis_and_exports_validation(tmp_path: Path) -> None:
    package_dir = tmp_path / "python_api_package" / "hiveio_api"
    template_dir = Path(__file__).parents[2] / "python_api_package" / "templates"
    generate_root_package(["block_api", "condenser_api"], tmp_path, template_dir)

    init_source = (package_dir / "__init__.py").read_text()
    assert not (package_dir / "_optional.py").exists()
    assert '    "block_api",\n    "condenser_api",\n)' in init_source  # `APIS`
    assert "register_validation_models(" not in init_source  # validation models of APIs are found without it
    assert '    "validate_schema",' in init_source
