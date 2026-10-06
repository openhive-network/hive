"""
Checks of the generated hiveio_api package.

Requires the package to be generated first (generate_api_packages.sh), skipped otherwise.
"""

from __future__ import annotations

import ast
import importlib
import json
import subprocess
import sys
from pathlib import Path
from typing import Any

import pytest

PACKAGE_ROOT = Path(__file__).parents[2] / "python_api_package"
PACKAGE_DIR = PACKAGE_ROOT / "hiveio_api"
OPENAPI = Path(__file__).parents[3] / "documentation" / "openapi.json"

pytestmark = pytest.mark.skipif(
    not (PACKAGE_DIR / "database_api" / "database_api_description.py").exists(),
    reason="hiveio_api package is not generated",
)


@pytest.fixture(scope="module", autouse=True)
def _package_on_path() -> None:
    if str(PACKAGE_ROOT) not in sys.path:
        sys.path.insert(0, str(PACKAGE_ROOT))


def example_of(endpoint: str) -> Any:
    spec = json.loads(OPENAPI.read_text())
    return spec["paths"][endpoint]["post"]["responses"]["200"]["content"]["application/json"]["example"]


def imported_modules(path: Path) -> set[str]:
    return {
        node.module
        for node in ast.parse(path.read_text()).body
        if isinstance(node, ast.ImportFrom) and node.module is not None
    }


@pytest.mark.parametrize("description", sorted(PACKAGE_DIR.glob("*/*_description.py")), ids=lambda path: path.stem)
def test_public_models_use_only_builtin_aliases(description: Path) -> None:
    # ACT
    modules = imported_modules(description)

    # ASSERT
    assert modules <= {
        "__future__",
        "dataclasses",
        "typing",
        "hiveio_api._base",
        "hiveio_api.common",
        "schemas.fields.base_type_mappings",
    }, modules


def test_validation_models_derive_from_preconfigured_base_model() -> None:
    # ARRANGE
    from schemas._preconfigured_base_model import PreconfiguredBaseModel

    module = importlib.import_module("hiveio_api._validation.database_api")

    # ACT
    result_model, is_array = module.ENDPOINT_RESULTS["find_accounts"]

    # ASSERT
    assert issubclass(result_model, PreconfiguredBaseModel)
    assert is_array is False


def test_importing_and_decoding_does_not_load_validation_models() -> None:
    # ARRANGE
    script = (
        "import json, sys\n"
        f"sys.path.insert(0, {str(PACKAGE_ROOT)!r})\n"
        "from typing import get_type_hints\n"
        "from hiveio_api.database_api import DatabaseApi\n"
        "from schemas.jsonrpc import get_response_model\n"
        "expected = get_type_hints(DatabaseApi.find_accounts)['return']\n"
        "get_response_model(expected, json.dumps({'id': 1, 'jsonrpc': '2.0', 'result': {'accounts': []}}), 'hf26')\n"
        "print([name for name in sys.modules if name.startswith('hiveio_api._validation')])\n"
    )

    # ACT
    result = subprocess.run([sys.executable, "-c", script], capture_output=True, text=True, check=True)

    # ASSERT
    assert result.stdout.strip() == "[]"


@pytest.mark.parametrize(
    "endpoint",
    [
        "database_api.find_accounts",
        "database_api.get_dynamic_global_properties",
        "condenser_api.get_dynamic_global_properties",
        "database_api.find_witnesses",
        "block_api.get_block_range",
        "account_history_api.get_account_history",
        "condenser_api.get_block",
        "condenser_api.get_witness_by_account",
    ],
)
def test_examples_from_openapi_are_valid(endpoint: str) -> None:
    # ARRANGE
    import hiveio_api

    # ACT
    errors = hiveio_api.validate_schema(example_of(endpoint), endpoint)

    # ASSERT
    assert errors == []


def test_invalid_response_is_reported_with_paths() -> None:
    # ARRANGE
    import hiveio_api

    response = example_of("database_api.find_accounts")
    response["accounts"][0]["name"] = "Not A Valid Name!"
    response["accounts"][0]["balance"] = {"amount": "1", "precision": 3, "nai": "@@000000013"}

    # ACT
    errors = hiveio_api.validate_schema(response, "database_api.find_accounts")

    # ASSERT
    assert {error.path for error in errors} == {"$.accounts[0].name", "$.accounts[0].balance"}


def test_endpoint_given_as_method_and_public_model_input() -> None:
    # ARRANGE
    import hiveio_api
    from hiveio_api.database_api import DatabaseApi, FindAccountsResponse

    response = FindAccountsResponse.from_builtins(example_of("database_api.find_accounts"), warn=False)

    # ACT & ASSERT
    assert hiveio_api.validate_schema(response, DatabaseApi.find_accounts) == []
