"""Checks of the generated hiveio_api package (see `generated_package`)."""

from __future__ import annotations

import ast
import asyncio
import importlib
import json
import subprocess
import sys
from typing import TYPE_CHECKING, get_type_hints

import pytest

from tests.generated_package import OPENAPI, PACKAGE_DIR, PACKAGE_ROOT, require_generated_package

if TYPE_CHECKING:
    from pathlib import Path

require_generated_package()


def _member(node: object, *keys: str) -> object:
    """Member of parsed JSON under the given keys of nested objects."""
    for key in keys:
        assert isinstance(node, dict), f"Expected an object with `{key}`"
        node = node[key]
    return node


def _response_content(spec: object, endpoint: str) -> dict[str, object]:
    content = _member(spec, "paths", endpoint, "post", "responses", "200", "content", "application/json")
    assert isinstance(content, dict)
    return content


def example_of(endpoint: str) -> object:
    return _response_content(json.loads(OPENAPI.read_text()), endpoint)["example"]


def endpoints_with_examples() -> list[str]:
    spec = json.loads(OPENAPI.read_text())
    return sorted(endpoint for endpoint in spec["paths"] if "example" in _response_content(spec, endpoint))


def imported_modules(path: Path) -> set[str]:
    return {
        node.module
        for node in ast.parse(path.read_text()).body
        if isinstance(node, ast.ImportFrom) and node.module is not None
    }


@pytest.mark.parametrize(
    "description",
    [*sorted(PACKAGE_DIR.glob("*/*_description.py")), PACKAGE_DIR / "operations.py"],
    ids=lambda path: path.stem,
)
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
        "hiveio_api.operations",
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


def test_importing_and_building_responses_does_not_load_validation_machinery() -> None:
    # ARRANGE
    script = (
        "import sys\n"
        f"sys.path.insert(0, {str(PACKAGE_ROOT)!r})\n"
        "from typing import get_type_hints\n"
        "from hiveio_api._communication.jsonrpc import build_response\n"
        "from hiveio_api.database_api import DatabaseApi\n"
        "expected = get_type_hints(DatabaseApi.find_accounts)['return']\n"
        "build_response(expected, {'id': 1, 'jsonrpc': '2.0', 'result': {'accounts': []}})\n"
        "print(sorted(name for name in sys.modules if name.startswith(('hiveio_api._validation', 'schemas.validation'))))\n"
    )

    # ACT
    result = subprocess.run([sys.executable, "-c", script], capture_output=True, text=True, check=True)

    # ASSERT
    assert result.stdout.strip() == "[]"


@pytest.mark.parametrize("endpoint", endpoints_with_examples())
def test_examples_from_openapi_are_valid(endpoint: str) -> None:
    # ARRANGE
    import hiveio_api

    # ACT
    errors = hiveio_api.validate_schema(example_of(endpoint), endpoint)

    # ASSERT
    assert errors == []


def _client_class(api: str) -> type[object]:
    module = importlib.import_module(f"hiveio_api.{api}.{api}_client")
    return next(
        obj
        for obj in vars(module).values()
        if isinstance(obj, type) and obj.__module__ == module.__name__ and hasattr(obj, "__validation_module__")
    )


@pytest.mark.parametrize("endpoint", endpoints_with_examples())
def test_examples_built_as_public_models_stay_valid(endpoint: str) -> None:
    """Public models keep everything needed to restore the response (e.g. keys which are not identifiers)."""
    # ARRANGE
    import hiveio_api
    from hiveio_api._communication.jsonrpc import JSONRPCResult, build_response

    api, _, method_name = endpoint.rpartition(".")
    method = getattr(_client_class(api), method_name)
    response = build_response(
        get_type_hints(method)["return"], {"id": 0, "jsonrpc": "2.0", "result": example_of(endpoint)}
    )
    assert isinstance(response, JSONRPCResult)

    # ACT
    errors = hiveio_api.validate_schema(response.result, method)

    # ASSERT
    assert errors == []


def test_invalid_response_is_reported_with_paths() -> None:
    # ARRANGE
    import hiveio_api

    response = example_of("database_api.find_accounts")
    assert isinstance(response, dict)
    response["accounts"][0]["name"] = "Not A Valid Name!"
    response["accounts"][0]["balance"] = {"amount": "1", "precision": 3, "nai": "@@000000013"}

    # ACT
    errors = hiveio_api.validate_schema(response, "database_api.find_accounts")

    # ASSERT
    assert {error.path for error in errors} == {"$.accounts[0].name", "$.accounts[0].balance"}


@pytest.mark.parametrize(
    ("endpoint", "balance_in_other_form"),
    [
        ("database_api.find_accounts", "1.000 HIVE"),
        ("condenser_api.get_accounts", {"amount": "1000", "precision": 3, "nai": "@@000000021"}),
    ],
)
def test_assets_are_validated_in_form_of_the_api(endpoint: str, balance_in_other_form: object) -> None:
    """Validation models have assets of fixed form (NAI or legacy) - no serialization of the API is needed."""
    # ARRANGE
    import hiveio_api

    response = example_of(endpoint)
    accounts = response["accounts"] if isinstance(response, dict) else response
    assert isinstance(accounts, list)
    accounts[0]["balance"] = balance_in_other_form
    path = "$.accounts[0].balance" if isinstance(response, dict) else "$[0].balance"

    # ACT
    errors = hiveio_api.validate_schema(response, endpoint)

    # ASSERT
    assert [error.path for error in errors] == [path]


def test_public_models_are_sent_with_keys_of_the_api() -> None:
    # ARRANGE
    from hiveio_api._apis.abc.sendable import AsyncSendable
    from hiveio_api._communication.jsonrpc import ExpectResultT, JSONRPCResult, build_response
    from hiveio_api.network_broadcast_api import NetworkBroadcastApi
    from hiveio_api.operations import Hf26Transaction

    class CapturingOwner(AsyncSendable):
        data: str | None = None

        def is_testnet(self) -> bool:
            return True

        async def _async_send(
            self, *, expected_type: type[ExpectResultT], data: str | None = None, **_: object
        ) -> JSONRPCResult[ExpectResultT]:
            self.data = data
            response = build_response(expected_type, {"id": 0, "jsonrpc": "2.0", "result": {}})
            assert isinstance(response, JSONRPCResult)
            return response

    transaction = {
        "ref_block_num": 1,
        "ref_block_prefix": 2,
        "expiration": "2026-01-01T00:00:00",
        "operations": [
            {
                "type": "transfer_operation",
                "value": {
                    "from": "alice",
                    "to": "bob",
                    "amount": {"amount": "1", "precision": 3, "nai": "@@000000021"},
                    "memo": "",
                },
            },
            {
                "type": "custom_json_operation",
                "value": {"required_auths": [], "required_posting_auths": ["alice"], "id": "follow", "json": "[]"},
            },
            {
                "type": "account_update2_operation",
                "value": {"account": "alice", "json_metadata": "{}", "posting_json_metadata": "", "extensions": []},
            },
        ],
        "extensions": [],
        "signatures": [],
    }
    owner = CapturingOwner()

    # ACT
    asyncio.run(
        NetworkBroadcastApi(owner).broadcast_transaction(
            trx=Hf26Transaction.from_builtins(transaction), max_block_age=-1
        )
    )

    # ASSERT
    assert owner.data is not None
    assert json.loads(owner.data)["params"] == {"trx": transaction, "max_block_age": -1}


def test_endpoint_given_as_method_and_public_model_input() -> None:
    # ARRANGE
    import hiveio_api
    from hiveio_api.database_api import DatabaseApi, FindAccountsResponse

    example = example_of("database_api.find_accounts")
    assert isinstance(example, dict)
    response = FindAccountsResponse.from_builtins(example, warn=False)

    # ACT & ASSERT
    assert hiveio_api.validate_schema(response, DatabaseApi.find_accounts) == []
