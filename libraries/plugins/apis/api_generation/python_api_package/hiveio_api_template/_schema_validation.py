"""
Explicit, opt-in validation of API responses.

API calls never validate responses. An application which wants to check a response calls `validate_schema`, either
with the validation model generated for the endpoint or with its own model (e.g. describing only the subset of fields
it uses).

Validation models of an API are found:

- per API name, registered with `register_validation_models` (used when the endpoint is given as
  `"api_name.method"`) - APIs of hiveio_api are found without registration (`hiveio_api._validation.<api>`),
- per client class, with the `__validation_module__` class attribute (used when the endpoint is given as a method,
  e.g. `chain.api.condenser_api.get_accounts`; this works for any API collection joined with `extends`).

A validation module exposes `ENDPOINT_RESULTS: dict[str, tuple[type | None, bool]]` (method name -> result model,
whether the result is an array).

A validation mask of an endpoint (`set_validation_mask` of endpoints) limits the validation of its responses to the
fields given by the mask - the generated model reduced to them, compiled when the mask is set (`HiveAsyncApi`).

Hive types of validation models have a fixed JSON form (e.g. legacy assets of condenser_api are `LegacyAssetHive`),
so the serialization of an API does not have to be known.
"""

from __future__ import annotations

import importlib
import importlib.util
import json
import types
from collections.abc import Mapping
from threading import Lock
from typing import TYPE_CHECKING, Final, cast

from hiveio_api._errors import UnknownEndpointError

if TYPE_CHECKING:
    from collections.abc import Callable
    from types import ModuleType

    from hiveio_api.validation import SchemaError

__all__ = [
    "compile_validation_mask",
    "register_validation_models",
    "validate_json",
    "validate_schema",
]

_VALIDATION_PACKAGE: Final[str] = "hiveio_api._validation."
"""Validation models of APIs of hiveio_api - `hiveio_api._validation.<api>`, found without registration."""
_registry: dict[str, str] = {}
_registry_lock = Lock()


def register_validation_models(api_name: str, module_path: str) -> None:
    """
    Register the module with validation models of the given API.

    Args:
        api_name: Name of the API, e.g. `condenser_api`.
        module_path: Dotted path of the validation module (imported lazily, on first validation).
    """
    with _registry_lock:
        _registry[api_name] = module_path


def validate_schema(
    response: object, endpoint: str | Callable[..., object], *, model: object | None = None
) -> list[SchemaError]:
    """
    Validate a response of the given endpoint.

    Args:
        response: Response to validate - builtins (e.g. from `json.loads`), raw JSON as `bytes` (a `str` is a JSON
            string), or result returned by an API call (public models are converted back to builtins, including
            undeclared fields).
        endpoint: `"api_name.method"` or the API method itself (e.g. `chain.api.condenser_api.get_accounts`).
        model: Type to validate against instead of the generated validation model (e.g. `list[MyAccount]`).

    Returns:
        All found problems, empty list when the response matches the model.

    Raises:
        hiveio_api.UnknownEndpointError: When `model` is not given and there is no validation model for the endpoint.
    """
    return validate_json(_response_to_builtins(response), endpoint, model=model)


def validate_json(
    data: object, endpoint: str | Callable[..., object], *, model: object | None = None
) -> list[SchemaError]:
    """
    Validate a response of the given endpoint given as JSON builtins (e.g. from `json.loads`) - as they are, no copy.

    Unlike `validate_schema`, public models are not looked for in the data (that needs a walk over all of it).

    Args:
        data: Response to validate - JSON builtins.
        endpoint: `"api_name.method"` or the API method itself (e.g. `chain.api.condenser_api.get_accounts`).
        model: Type to validate against instead of the generated validation model (e.g. `list[MyAccount]`).

    Returns:
        All found problems, empty list when the response matches the model. Without `model` only the fields of the
        validation mask of the endpoint are validated, if it has one - of the method given, of the request being sent
        for `"api_name.method"` (validation of responses by callbacks of requests).

    Raises:
        hiveio_api.UnknownEndpointError: When `model` is not given and there is no validation model for the endpoint.
    """
    from hiveio_api.validation import validate

    return validate(data, _target_model(endpoint, model))


def compile_validation_mask(api_class: type[object], endpoint_name: str, mask: Mapping[str, object]) -> object:
    """
    The validation model of the endpoint reduced to the fields given by the validation mask (`set_validation_mask`).

    Called when the mask is set - a wrong mask raises at once, requests use the built model.

    Args:
        api_class: Class of the API (with `__validation_module__`).
        endpoint_name: Name of the endpoint (the method), e.g. `get_block`.
        mask: The validation mask - a key per field of the result model, `True` or the mask of the model of the field.

    Raises:
        hiveio_api.UnknownEndpointError: When there is no validation model for the endpoint.
        ValueError: When the mask names a field the model does not have, or selects no fields.
        TypeError: When a value of the mask is not `True` / a dict, or a dict is given for a field without a model.
    """
    from hiveio_api.validation._masks import masked_model  # msgspec - only when masks are set

    return masked_model(_generated_model(_validation_module_of(api_class), endpoint_name), mask)


def _response_to_builtins(response: object) -> object:
    if isinstance(response, bytes | bytearray):
        return json.loads(response)

    from hiveio_api._base import to_builtins  # models (dataclasses) are not loaded by `import hiveio_api`

    return to_builtins(response)


def _target_model(endpoint: str | Callable[..., object], model: object | None) -> object:
    if model is not None:
        return model
    compiled = _compiled_validation_mask(endpoint)
    if compiled is not None and not isinstance(compiled, Mapping):  # compiled by `HiveAsyncApi` - a reduced model
        return compiled
    module_path, method = _resolve_endpoint(endpoint)
    generated = _generated_model(module_path, method)
    if compiled is None:
        return generated

    from hiveio_api.validation._masks import masked_model  # msgspec - only when validating

    return masked_model(generated, cast("Mapping[str, object]", compiled))  # an API which does not compile masks


def _compiled_validation_mask(endpoint: str | Callable[..., object]) -> object | None:
    """
    Compiled validation mask of the endpoint (`set_validation_mask` of endpoints) - of the endpoint given as a method,
    of the request being sent for the endpoint given as `"api_name.method"` (validation of responses by callbacks).
    """
    if isinstance(endpoint, str):
        from hiveio_api_client import compiled_validation_mask_of

        return compiled_validation_mask_of(endpoint)
    return getattr(endpoint, "compiled_validation_mask", None)


def _validation_module_of(api_class: type[object]) -> str:
    module_path = getattr(api_class, "__validation_module__", None)
    if not isinstance(module_path, str):
        raise UnknownEndpointError(f"API class `{api_class.__qualname__}` does not define `__validation_module__`.")
    return module_path


def _resolve_endpoint(endpoint: str | Callable[..., object]) -> tuple[str, str]:
    if isinstance(endpoint, str):
        api_name, _, method = endpoint.rpartition(".")
        with _registry_lock:
            module_path = _registry.get(api_name)
        if module_path is None:
            module_path = _VALIDATION_PACKAGE + api_name  # APIs of hiveio_api - not registered
            if not api_name.isidentifier() or importlib.util.find_spec(module_path) is None:
                raise UnknownEndpointError(f"No validation models registered for API `{api_name}` (endpoint `{endpoint}`).")
        return module_path, method

    return _validation_module_of(_owner_class(endpoint)), endpoint.__name__


def _owner_class(endpoint: Callable[..., object]) -> type[object]:
    bound_to = getattr(endpoint, "__self__", None)
    if bound_to is not None:
        return bound_to if isinstance(bound_to, type) else type(bound_to)

    module = importlib.import_module(endpoint.__module__)
    owner: object = module
    for part in endpoint.__qualname__.split(".")[:-1]:
        owner = getattr(owner, part)
    if not isinstance(owner, type):
        raise UnknownEndpointError(f"Cannot find API class of `{endpoint.__qualname__}`.")
    return owner


def _generated_model(module_path: str, method: str) -> object:
    module: ModuleType = importlib.import_module(module_path)
    results: Mapping[str, tuple[object, bool]] = module.ENDPOINT_RESULTS  # generated - see the module docstring
    if method not in results:
        raise UnknownEndpointError(f"No validation model for method `{method}` in `{module_path}`.")
    result_model, is_array = results[method]
    return types.GenericAlias(list, (result_model,)) if is_array else result_model  # `list[result_model]`
