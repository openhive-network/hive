"""Schema filters of the generated package and validation masks of its endpoints (`set_validation_mask`)."""

from __future__ import annotations

import ast
import dataclasses
import importlib
import re
import typing
import warnings
from typing import TYPE_CHECKING, ForwardRef, cast, get_type_hints

import pytest

from api_generation.common import available_apis
from api_generation.schema_filters import FILTERS_ALIAS, filter_name, nested_model
from tests.generated_package import PACKAGE_DIR, require_generated_package

require_generated_package()

from hiveio_api import validate_json, validate_schema  # noqa: E402
from hiveio_api._hive_api import HiveAsyncApi  # noqa: E402
from hiveio_api.block_api.block_api_client import BlockApi  # noqa: E402
from hiveio_api_client import ClassFilterInterface, compiled_validation_mask_of  # noqa: E402
from hiveio_api_client._apis.abc.validation_masks import compiled_validation_masks  # noqa: E402

if TYPE_CHECKING:
    from hiveio_api_client import AsyncSendable

HEADER = {
    "previous": "bad-previous",
    "timestamp": "2016-03-24T16:05:00",
    "witness": "X!",
    "transaction_merkle_root": "0" * 40,
    "extensions": [],
}
"""Result of `block_api.get_block_header` - `previous` and `witness` are wrong."""


def _api[ApiT: HiveAsyncApi](api_class: type[ApiT]) -> ApiT:
    return api_class(owner=cast("AsyncSendable", object()))  # no requests are sent


def _clients() -> list[type[HiveAsyncApi]]:
    clients: list[type[HiveAsyncApi]] = []
    for api in available_apis:
        module = importlib.import_module(f"hiveio_api.{api}.{api}_client")
        clients.extend(
            value
            for value in vars(module).values()
            if isinstance(value, type) and issubclass(value, HiveAsyncApi) and value is not HiveAsyncApi
        )
    return clients


def _endpoint_results(client: type[HiveAsyncApi]) -> list[tuple[str, type]]:
    """Endpoints of the client with result models (also of lists of models) - with the models."""
    results = []
    for name, value in vars(client).items():
        function = getattr(value, "__wrapped__", None)
        if function is None:
            continue
        model = nested_model(get_type_hints(function)["return"])
        if model is not None:
            results.append((name, model))
    return results


def _mask_of_all_fields(model: type, path: tuple[type, ...] = ()) -> dict[str, object]:
    """Mask naming each field of the model - fields with models of schema filters as masks of all their fields."""
    hints = get_type_hints(model)
    mask: dict[str, object] = {}
    for field in dataclasses.fields(model):
        nested = nested_model(hints[field.name])
        if nested is not None and issubclass(nested, ClassFilterInterface) and nested not in path:
            mask[field.name] = _mask_of_all_fields(nested, (*path, model))
        else:
            mask[field.name] = True
    return mask


CLIENTS = _clients()
ENDPOINTS = [(client, name, model) for client in CLIENTS for name, model in _endpoint_results(client)]
MASKABLE_ENDPOINTS = [(client, name, model) for client, name, model in ENDPOINTS if dataclasses.fields(model)]
"""Endpoints returning models with fields - models without fields (empty results) have no schema filters."""


def test_endpoints_with_result_models_are_found() -> None:
    # ASSERT - the cases of the tests below
    assert len(CLIENTS) == len(available_apis)
    assert len(MASKABLE_ENDPOINTS) > 100  # most endpoints of all APIs return models


@pytest.mark.parametrize(
    ("client", "endpoint", "model"), MASKABLE_ENDPOINTS, ids=lambda value: getattr(value, "__name__", value)
)
def test_mask_of_all_fields_compiles_for_each_endpoint(client: type[HiveAsyncApi], endpoint: str, model: type) -> None:
    # ARRANGE - public models and validation models have the same fields of the same structure
    api = _api(client)

    # ACT
    getattr(api, endpoint).set_validation_mask(_mask_of_all_fields(model))

    # ASSERT
    assert getattr(api, endpoint).compiled_validation_mask is not None


@pytest.mark.parametrize(
    ("client", "endpoint", "model"), MASKABLE_ENDPOINTS, ids=lambda value: getattr(value, "__name__", value)
)
def test_result_models_have_schema_filters_with_their_fields(
    client: type[HiveAsyncApi], endpoint: str, model: type
) -> None:
    # ACT
    bases = [base for base in getattr(model, "__orig_bases__", ()) if typing.get_origin(base) is ClassFilterInterface]
    stub = PACKAGE_DIR.joinpath("_schema_filters", *model.__module__.split(".")[1:]).with_suffix(".pyi")
    filters = {
        node.name: [statement.target.id for statement in node.body if isinstance(statement, ast.AnnAssign)]  # type: ignore[union-attr]
        for node in ast.parse(stub.read_text()).body
        if isinstance(node, ast.ClassDef)
    }

    # ASSERT
    assert [typing.get_args(base) for base in bases] == [(ForwardRef(f"{FILTERS_ALIAS}.{filter_name(model)}"),)]
    assert filters[filter_name(model)] == [field.name for field in dataclasses.fields(model)]


def test_result_models_without_fields_have_no_schema_filters() -> None:
    # ACT
    without_fields = {model for _, _, model in ENDPOINTS if not dataclasses.fields(model)}

    # ASSERT
    assert without_fields, "e.g. results of broadcast_transaction"
    assert not [model for model in without_fields if issubclass(model, ClassFilterInterface)]


def test_mask_limits_validation_to_its_fields() -> None:
    # ARRANGE
    api = _api(BlockApi)
    data = {"header": HEADER}

    # ACT
    api.get_block_header.set_validation_mask({"header": {"timestamp": True, "extensions": True}})
    errors_of_correct_fields = validate_schema(data, api.get_block_header)
    api.get_block_header.set_validation_mask({"header": {"witness": True}})
    errors_of_wrong_field = validate_schema(data, api.get_block_header)

    # ASSERT
    assert errors_of_correct_fields == []
    assert [error.path for error in errors_of_wrong_field] == ["$.header.witness"]
    assert {error.path for error in validate_schema(data, "block_api.get_block_header")} == {
        "$.header.previous",
        "$.header.witness",
    }, "endpoints given by names (outside of requests) are validated whole"


def test_mask_of_a_list_result_limits_validation_of_each_item() -> None:
    # ARRANGE
    api = _api(BlockApi)
    blocks = {"blocks": [{"block_id": "1" * 40, "transactions": [{"signatures": ["bad"], "operations": [{}]}]}]}

    # ACT
    api.get_block_range.set_validation_mask({"blocks": {"block_id": True, "transactions": {"signatures": True}}})
    errors = validate_schema(blocks, api.get_block_range)

    # ASSERT
    assert [error.path for error in errors] == ["$.blocks[0].transactions[0].signatures[0]"]


def test_compiled_mask_of_the_request_being_sent_limits_validate_json() -> None:
    # ARRANGE - as validation of responses by callbacks (e.g. SchemaValidationRules of beekeepy)
    api = _api(BlockApi)
    api.get_block_header.set_validation_mask({"header": {"timestamp": True}})
    compiled = api.get_block_header.compiled_validation_mask

    # ACT
    with compiled_validation_masks({"block_api.get_block_header": compiled}):
        assert compiled_validation_mask_of("block_api.get_block_header") is compiled
        errors = validate_json({"header": HEADER}, "block_api.get_block_header")

    # ASSERT
    assert errors == []


@pytest.mark.parametrize(
    ("mask", "error", "message"),
    [
        ({"header": {"timestamq": True}}, ValueError, "`BlockHeader` has no fields ['timestamq']"),
        ({"header": {}}, ValueError, "at `$.header` selects no fields"),
        ({}, ValueError, "at `$` selects no fields"),
        ({"header": {"witness": {"name": True}}}, TypeError, "at `$.header.witness`: the field has no model"),
        ({"header": {"witness": 1}}, TypeError, "at `$.header.witness` must be `True` or a dict, got `int`"),
    ],
)
def test_wrong_mask_raises_when_it_is_set(mask: dict[str, object], error: type[Exception], message: str) -> None:
    # ARRANGE
    endpoint = _api(BlockApi).get_block_header
    endpoint.set_validation_mask({"header": True})

    # ACT & ASSERT
    with pytest.raises(error, match=re.escape(message)):
        endpoint.set_validation_mask(mask)  # type: ignore[arg-type]
    assert endpoint.validation_mask == {"header": True}, "the previous mask stays"


def test_unions_of_models_take_only_true() -> None:
    # ARRANGE
    endpoint = _api(BlockApi).get_block

    # ACT & ASSERT
    with pytest.raises(TypeError, match=re.escape("`$.block.transactions.operations`: the field is a union of models")):
        endpoint.set_validation_mask({"block": {"transactions": {"operations": {"type": True}}}})  # type: ignore[arg-type]


def test_false_in_mask_warns_at_the_caller_and_is_ignored() -> None:
    # ARRANGE
    endpoint = _api(BlockApi).get_block_header

    # ACT
    with warnings.catch_warnings(record=True) as caught:
        warnings.simplefilter("always")
        endpoint.set_validation_mask({"header": {"witness": True, "previous": False}})

    # ASSERT
    assert [(str(warning.message), warning.filename) for warning in caught] == [
        (
            "`False` in the validation mask at `$.header.previous` has no effect - fields not given are not validated.",
            __file__,
        )
    ]
    assert [error.path for error in validate_schema({"header": HEADER}, endpoint)] == ["$.header.witness"]
