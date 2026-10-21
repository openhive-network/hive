"""Checks of the generated hiveio_api package (see `generated_package`)."""

from __future__ import annotations

import ast
import asyncio
import dataclasses
import importlib
import json
import subprocess
import sys
import tomllib
import types
from typing import TYPE_CHECKING, Union, get_args, get_origin, get_type_hints

import pytest

from tests.generated_package import (
    OPENAPI,
    PACKAGE_DIR,
    PACKAGE_ROOT,
    TEMPLATE_DIR,
    generated_modules,
    require_generated_package,
)

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


def test_package_holds_its_template_unchanged() -> None:
    # ARRANGE
    template = sorted(path for path in TEMPLATE_DIR.rglob("*") if path.is_file() and "__pycache__" not in path.parts)

    # ACT & ASSERT - hand-written modules are copied, never generated over
    assert template
    for path in template:
        assert (PACKAGE_DIR / path.relative_to(TEMPLATE_DIR)).read_bytes() == path.read_bytes(), path


def imported_modules(path: Path) -> set[str]:
    """Modules imported by the module - relative imports (`from .values import`) by their absolute names."""
    package = ".".join(path.relative_to(PACKAGE_DIR.parent).parent.parts)
    return {
        f"{package}.{node.module}" if node.level else node.module
        for node in ast.parse(path.read_text()).body
        if isinstance(node, ast.ImportFrom) and node.module is not None
    }


@pytest.mark.parametrize(
    "description",
    [
        *sorted(PACKAGE_DIR.glob("*/*_description.py")),
        *sorted((PACKAGE_DIR / "common").glob("[!_]*.py")),
        *generated_modules(sorted((PACKAGE_DIR / "operations").glob("[!_]*.py"))),
    ],
    ids=lambda path: path.stem,
)
def test_public_models_use_only_builtin_aliases(description: Path) -> None:
    # ACT
    modules = {module for module in imported_modules(description) if not module.startswith("hiveio_api.common.")}

    # ASSERT - shared models (`hiveio_api.common.<model>`) are checked as the other modules
    assert modules <= {
        "__future__",
        "dataclasses",
        "typing",
        "hiveio_api",  # the common package (`from hiveio_api import common`)
        "hiveio_api._base",
        "hiveio_api.common",
        "hiveio_api.operations.values",
        "hiveio_api.operations.hf26",
        "hiveio_api.operations.legacy",
        "schemas.fields.public",
    }, modules


def test_api_loads_only_the_shared_models_it_uses() -> None:
    # ARRANGE
    code = (
        "import sys\n"
        "import hiveio_api.block_api.block_api_description as description\n"
        "from hiveio_api.common import Block\n"
        "shared = sorted(name for name in sys.modules if name.startswith('hiveio_api.common.'))\n"
        "print(shared, description.Block is Block)\n"
    )

    # ACT
    result = subprocess.run(
        [sys.executable, "-c", code], capture_output=True, text=True, check=True, cwd=PACKAGE_DIR.parent
    )

    # ASSERT
    loaded, same_class = result.stdout.strip().rsplit(" ", 1)
    assert same_class == "True"
    modules = ast.literal_eval(loaded)
    assert "hiveio_api.common.block" in modules
    assert len(modules) < len(list((PACKAGE_DIR / "common").glob("*.py"))) - 1  # not all shared models


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


def test_importing_the_package_does_not_load_models() -> None:
    # ARRANGE
    script = (
        "import sys\n"
        f"sys.path.insert(0, {str(PACKAGE_ROOT)!r})\n"
        "import hiveio_api\n"
        "print(sorted(name for name in sys.modules if name == 'hiveio_api._base' or name.startswith('schemas')))\n"
        "print(hiveio_api.Asset.__name__, hiveio_api.HiveDateTime.__name__, hiveio_api.to_builtins.__name__)\n"
        "print(hiveio_api.build.__name__, hiveio_api.rc_api.__name__)\n"
    )

    # ACT
    result = subprocess.run([sys.executable, "-c", script], capture_output=True, text=True, check=True)

    # ASSERT - the exported names (also subpackages of APIs) are still available, imported when used
    assert result.stdout.splitlines() == ["[]", "Asset HiveDateTime to_builtins", "build hiveio_api.rc_api"]


def test_operations_are_made_from_their_values_in_both_forms() -> None:
    # ARRANGE
    from hiveio_api import Asset
    from hiveio_api.operations import (
        Hf26TransferOperation,
        TransferOperation,
        TransferOperationLegacy,
        VoteOperation,
        hf26_operation,
        legacy_operation,
    )

    transfer = TransferOperation(from_="alice", to="bob", amount=Asset.hive.satoshi(1000), memo="")
    vote = VoteOperation(voter="alice", author="bob", permlink="post", weight=10000)

    # ACT
    hf26 = hf26_operation(transfer)
    legacy_transfer = legacy_operation(transfer)
    legacy_vote = legacy_operation(vote)

    # ASSERT
    assert isinstance(hf26, Hf26TransferOperation)
    assert hf26.value is transfer
    assert legacy_transfer[0] == "transfer"
    assert isinstance(legacy_transfer[1], TransferOperationLegacy)
    assert legacy_transfer[1].to_builtins()["amount"] == "1.000 HIVE"
    assert legacy_vote == ("vote", vote)
    assert legacy_vote[1] is vote  # no assets - the same value in both forms
    back = hf26_operation(legacy_transfer[1])  # any value of an operation (`OperationBase`), also of the legacy form
    assert isinstance(back, Hf26TransferOperation)
    assert isinstance(back.value, TransferOperation)
    assert back.value.to_builtins() == transfer.to_builtins()


def test_generator_and_package_depend_on_the_same_schemas() -> None:
    # ARRANGE - the generator writes models for the field types of the schemas the package depends on
    def schemas_of(pyproject: Path) -> object:
        return tomllib.loads(pyproject.read_text())["tool"]["poetry"]["dependencies"]["hiveio-schemas"]["version"]

    # ACT & ASSERT
    assert schemas_of(PACKAGE_ROOT / "pyproject.toml") == schemas_of(PACKAGE_ROOT.parent / "api_generation" / "pyproject.toml")


def test_infrastructure_of_clients_is_public_and_imported_when_used() -> None:
    # ARRANGE
    script = (
        "import sys\n"
        f"sys.path.insert(0, {str(PACKAGE_ROOT)!r})\n"
        "import hiveio_api.client as client\n"
        "print(sorted(name for name in sys.modules if name.startswith(('hiveio_api._apis', 'hiveio_api._communication'))))\n"
        "print(all(getattr(client, name) is not None for name in client.__all__))\n"
    )

    # ACT
    result = subprocess.run([sys.executable, "-c", script], capture_output=True, text=True, check=True)

    # ASSERT
    assert result.stdout.splitlines() == ["[]", "True"]


@pytest.mark.parametrize("endpoint", endpoints_with_examples())
def test_examples_from_openapi_are_valid(endpoint: str) -> None:
    # ARRANGE
    import hiveio_api

    # ACT
    errors = hiveio_api.validate_schema(example_of(endpoint), endpoint)

    # ASSERT
    assert errors == []


def _models_are_built(annotation: object, value: object) -> bool:
    """Whether the public models of the annotation (also in lists and values of dicts) are built in the value."""
    from hiveio_api import HiveModel

    if isinstance(annotation, type) and issubclass(annotation, HiveModel):
        if not isinstance(value, annotation):
            return False
        hints = get_type_hints(type(value))
        return all(
            _models_are_built(hints[field.name], getattr(value, field.name)) for field in dataclasses.fields(value)
        )
    if get_origin(annotation) is list and isinstance(value, list):
        return all(_models_are_built(get_args(annotation)[0], item) for item in value)
    if get_origin(annotation) is dict and isinstance(value, dict):
        return all(_models_are_built(get_args(annotation)[1], item) for item in value.values())
    if get_origin(annotation) in (Union, types.UnionType) and value is not None:
        members = [member for member in get_args(annotation) if member is not type(None)]
        return len(members) != 1 or _models_are_built(members[0], value)  # optional values, other unions unchecked
    return True  # builtins - checked by validation


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

    # ASSERT - built as public models, not kept as the parsed JSON
    assert errors == []
    assert _models_are_built(get_type_hints(method)["return"], response.result)


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
    assert {error.path for error in errors} == {"$.accounts[0].name", "$.accounts[0].balance.nai"}


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


def test_public_models_can_be_changed() -> None:
    # ARRANGE
    from hiveio_api.operations import TransferOperation

    amount = {"amount": "1000", "precision": 3, "nai": "@@000000021"}
    transfer = TransferOperation.from_builtins({"from": "alice", "to": "bob", "amount": amount, "memo": ""})

    # ACT
    transfer.memo = "changed"

    # ASSERT
    assert transfer.to_builtins()["memo"] == "changed"


def test_status_item_equals_its_name() -> None:
    # ARRANGE
    from hiveio_api.app_status_api import AppStatusGetAppStatusResponse

    data = {"timestamp": "2026-01-01T00:00:00", "status": "chain API ready"}
    app_status = AppStatusGetAppStatusResponse.from_builtins({"statuses": [data], "webservers": {}, "version": {}})
    (item,) = app_status.statuses

    # ACT & ASSERT
    assert "chain API ready" in app_status.statuses
    assert "entering live mode" not in app_status.statuses
    assert item == type(item).from_builtins(data)
    assert item != type(item).from_builtins({**data, "status": "syncing"})


def test_fields_missing_in_the_data_are_sent_once_set() -> None:
    # ARRANGE
    from hiveio_api.operations import CommentOptionsOperation

    data = {"author": "alice", "permlink": "post"}
    set_later = CommentOptionsOperation.from_builtins(data, warn=False)
    changed_in_place = CommentOptionsOperation.from_builtins(data, warn=False)
    left_alone = CommentOptionsOperation.from_builtins(data, warn=False)

    # ACT
    set_later.allow_votes = False
    changed_in_place.extensions.append({"type": "unknown", "value": {}})

    # ASSERT
    assert set_later.to_builtins()["allow_votes"] is False
    assert changed_in_place.to_builtins()["extensions"] == [{"type": "unknown", "value": {}}]
    assert left_alone.to_builtins() == data


def test_dates_given_by_default_are_hive_dates() -> None:
    # ARRANGE
    from hiveio_api import HiveDateTime
    from hiveio_api.operations import LimitOrderCreateOperation

    order = {
        "owner": "alice",
        "orderid": 1,
        "amount_to_sell": {"amount": "1000", "precision": 3, "nai": "@@000000021"},
        "min_to_receive": {"amount": "1000", "precision": 3, "nai": "@@000000013"},
        "fill_or_kill": False,
    }

    # ACT
    built = LimitOrderCreateOperation.from_builtins(order, warn=False)
    constructed = LimitOrderCreateOperation(
        owner="alice",
        orderid=1,
        amount_to_sell=built.amount_to_sell,
        min_to_receive=built.min_to_receive,
        fill_or_kill=False,
    )

    # ASSERT
    assert type(built.expiration) is HiveDateTime
    assert type(constructed.expiration) is HiveDateTime
    assert constructed.expiration == HiveDateTime("1969-12-31T23:59:59")


def test_public_models_are_sent_with_keys_of_the_api() -> None:
    # ARRANGE
    from hiveio_api._apis.abc.sendable import AsyncSendable
    from hiveio_api._communication.jsonrpc import ExpectResultT, JSONRPCResult, build_response
    from hiveio_api import Asset
    from hiveio_api.network_broadcast_api import NetworkBroadcastApi
    from hiveio_api.operations import Transaction, TransferOperation

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
    trx = Transaction.from_builtins(transaction)
    transfer = trx.operations[0].value
    assert isinstance(transfer, TransferOperation)
    transfer.amount = Asset("0.001 TESTS")  # the legacy form - sent in the NAI form of the field

    # ACT
    asyncio.run(NetworkBroadcastApi(owner).broadcast_transaction(trx=trx, max_block_age=-1))

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
