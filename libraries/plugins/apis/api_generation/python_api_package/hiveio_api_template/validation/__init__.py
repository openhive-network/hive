"""
Validation of JSON data (builtins) against msgspec models with Hive types from `hiveio_api.fields`.

Hive types have a fixed JSON form (e.g. `AssetHive` - NAI form, `LegacyAssetHive` - legacy form), so no serialization
has to be given.
"""

from __future__ import annotations

import re
import types
from dataclasses import dataclass
from typing import TYPE_CHECKING, Final, Union, get_args, get_origin

import msgspec

from hiveio_api.validation.decoders import dec_hook_hf26, first_use_guarded

if TYPE_CHECKING:
    from collections.abc import Callable

__all__ = [
    "SchemaError",
    "is_matching_model",
    "validate",
]

_ROOT_PATH: Final[str] = "$"
_MSGSPEC_PATH: Final[re.Pattern[str]] = re.compile(r" - at `\$([^`]*)`$")
"""
Location at the end of messages of `msgspec.ValidationError` - the format of msgspec 0.20, e.g.
"Expected `int`, got `str` - at `$.a[0]`". msgspec gives the location only in the message, the format is pinned by
`test_msgspec_reports_the_location_in_the_parsed_format`.
"""


@dataclass(frozen=True, slots=True)
class SchemaError:
    """Problem found in the data by `validate`."""

    path: str
    """Location of the problem in the data, e.g. `$[0].balance`."""
    message: str


def validate(data: object, model: object) -> list[SchemaError]:
    """
    Validate data against the given model.

    Args:
        data: Builtins, e.g. from `json.loads`.
        model: Type to validate against, e.g. `list[GetAccountsResponse]`.

    Returns:
        All found problems, empty list when the data matches the model.
    """
    failure = _failure(data, model)
    if failure is None:
        return []
    errors: list[SchemaError] = []
    _collect_errors(data, model, _ROOT_PATH, errors, failure)
    return errors


def is_matching_model(data: object, field_type: Callable[[object], object]) -> bool:
    """Check if the data matches the given field type (e.g. `AccountName`, `Asset`) - the type accepts it."""
    try:
        field_type(data)
    except (msgspec.ValidationError, ValueError, TypeError):  # invalid value / value of a wrong type
        return False
    return True


@dataclass(frozen=True, slots=True)
class _Failure:
    """The first problem of a value - msgspec stops at it."""

    location: str
    """Location of the problem in the value in the notation of msgspec, e.g. `[3].balance` (empty - in the value)."""
    message: str

    def inside(self, segment: str) -> _Failure | None:
        """The problem of the item of the value at the segment (e.g. `[3]`, `.balance`), None when it is not there."""
        rest = self.location.removeprefix(segment)
        if rest == self.location or rest[:1] not in ("", ".", "["):
            return None
        return _Failure(rest, self.message)


def _convert(data: object, type_: object) -> object:
    with first_use_guarded(type_):
        return msgspec.convert(data, type=type_, dec_hook=dec_hook_hf26)


def _failure(data: object, type_: object) -> _Failure | None:
    try:
        _convert(data, type_)
    except msgspec.ValidationError as error:
        message = str(error)
        match = _MSGSPEC_PATH.search(message)
        return _Failure(match.group(1), message[: match.start()]) if match else _Failure("", message)
    return None


def _collect_errors(
    data: object, type_: object, path: str, errors: list[SchemaError], failure: _Failure | None = None
) -> None:
    """
    Collect the errors of the value - msgspec reports only the first one, so values with errors are searched for more.

    `failure` is the first problem of the value when it is already known (found by the conversion of a value containing
    it) - values on the path to a problem are not converted again.
    """
    if failure is None:
        failure = _failure(data, type_)
        if failure is None:
            return

    errors_before = len(errors)
    _descend(data, type_, path, errors, failure)
    if len(errors) == errors_before:  # nothing more precise found inside - report the problem found by msgspec
        errors.append(SchemaError(f"{path}{failure.location}", failure.message))


def _descend(data: object, type_: object, path: str, errors: list[SchemaError], failure: _Failure) -> None:
    if isinstance(type_, type) and issubclass(type_, msgspec.Struct) and isinstance(data, dict):
        _descend_struct(data, type_, path, errors, failure)
        return

    origin = get_origin(type_)
    args = get_args(type_)
    if origin in (list, tuple, set, frozenset) and args and isinstance(data, list):
        if origin is tuple and not (len(args) == 2 and args[1] is Ellipsis):  # noqa: PLR2004
            return
        for index, item in enumerate(data):
            _collect_errors(item, args[0], f"{path}[{index}]", errors, failure.inside(f"[{index}]"))
        return

    if origin is dict and len(args) == 2 and isinstance(data, dict):  # noqa: PLR2004
        for key, item in data.items():  # msgspec does not name keys in locations (`[...]`)
            _collect_errors(item, args[1], f"{path}.{key}", errors)
        return

    if origin in (Union, types.UnionType):
        structs = [arg for arg in args if isinstance(arg, type) and issubclass(arg, msgspec.Struct)]
        if isinstance(data, dict) and len(structs) == 1:
            _descend_struct(data, structs[0], path, errors, failure)


def _descend_struct(
    data: dict[str, object], type_: type[msgspec.Struct], path: str, errors: list[SchemaError], failure: _Failure
) -> None:
    fields = msgspec.structs.fields(type_)
    known_names = {field.encode_name for field in fields}
    failures = {name: inner for name in known_names if (inner := failure.inside(f".{name}")) is not None}
    if len(failures) > 1:  # location ambiguous (names with `.`) - the fields are converted
        failures.clear()
    for field in fields:
        name = field.encode_name
        if name in data:
            _collect_errors(data[name], field.type, f"{path}.{name}", errors, failures.get(name))
        elif field.required:
            errors.append(SchemaError(path, f"Object missing required field `{name}`"))

    if getattr(type_, "__struct_config__", None) is not None and type_.__struct_config__.forbid_unknown_fields:
        errors.extend(
            SchemaError(f"{path}.{key}", f"Object contains unknown field `{key}`")
            for key in data
            if key not in known_names
        )
