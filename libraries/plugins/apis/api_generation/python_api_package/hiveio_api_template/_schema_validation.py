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

A validation module exposes `ENDPOINT_RESULTS: dict[str, tuple[type | str, bool]]` (method name -> result model or
name of the module attribute holding it, whether the result is an array).

Hive types of validation models have a fixed JSON form (e.g. legacy assets of condenser_api are `LegacyAssetHive`),
so the serialization of an API does not have to be known.
"""

from __future__ import annotations

import importlib
import importlib.util
import json
from threading import Lock
from typing import TYPE_CHECKING, Final

from hiveio_api._errors import UnknownEndpointError

if TYPE_CHECKING:
    from collections.abc import Callable, Mapping
    from types import ModuleType

    from schemas.validation import SchemaError

__all__ = [
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

    Raises:
        hiveio_api.UnknownEndpointError: When `model` is not given and there is no validation model for the endpoint.
    """
    from schemas.validation import validate

    return validate(data, _target_model(endpoint, model))


def _response_to_builtins(response: object) -> object:
    if isinstance(response, bytes | bytearray):
        return json.loads(response)

    from hiveio_api._base import to_builtins  # models (dataclasses) are not loaded by `import hiveio_api`

    return to_builtins(response)


def _target_model(endpoint: str | Callable[..., object], model: object | None) -> object:
    if model is not None:
        return model
    module_path, method = _resolve_endpoint(endpoint)
    return _generated_model(module_path, method)


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

    owner = _owner_class(endpoint)
    module_path = getattr(owner, "__validation_module__", None)
    if not isinstance(module_path, str):
        raise UnknownEndpointError(f"API class `{owner.__qualname__}` does not define `__validation_module__`.")
    return module_path, endpoint.__name__


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
    results: Mapping[str, tuple[object, bool]] = module.ENDPOINT_RESULTS
    if method not in results:
        raise UnknownEndpointError(f"No validation model for method `{method}` in `{module_path}`.")
    result_model, is_array = results[method]
    if isinstance(result_model, str):
        result_model = getattr(module, result_model)
    return list[result_model] if is_array else result_model  # type: ignore[valid-type]
