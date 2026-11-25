"""
Validation models reduced to the fields given by validation masks of endpoints (`set_validation_mask` of endpoints).

A mask is a dict - a key per field of the model to validate, its value is `True` (the whole field) or the mask of the
model of the field (of items of a list, of values of a dict). The reduced model ignores fields not given in the mask.
Leaves of a mask are only `True`: an empty dict is an error (it is not known what to validate), `False` is dropped
with a warning (fields not given are not validated anyway). Unions of several models (e.g. operations) are validated
whole - they take only `True`.
"""

from __future__ import annotations

import os
import types
import warnings
from collections.abc import Mapping
from functools import lru_cache
from typing import Annotated, Final, Union, cast, get_args, get_origin

import hiveio_api_client
import msgspec

__all__ = ["masked_model"]

FrozenMask = tuple[tuple[str, "bool | FrozenMask"], ...]
"""A mask as a hashable value - sorted pairs of names of fields and `True` or masks of their models."""

_ROOT_PATH: Final[str] = "$"
_PACKAGE_FILES: Final[tuple[str, ...]] = (
    os.path.dirname(os.path.dirname(__file__)),
    os.path.dirname(hiveio_api_client.__file__),
)
"""Warnings about masks point at the first caller outside the packages (endpoints of hiveio_api_client set masks)."""
_NOT_VALUES: Final[tuple[object, ...]] = (type(None), msgspec.UnsetType)
"""Members of unions which are no values of the field (`None`, a field missing in the data)."""


def masked_model(model: object, mask: Mapping[str, object]) -> object:
    """
    The validation model reduced to the fields given by the mask.

    Args:
        model: Validation model of a result (a msgspec model, a list of them, ...).
        mask: Validation mask of the endpoint.

    Raises:
        ValueError: When the mask names a field the model does not have, or selects no fields.
        TypeError: When a value of the mask is not `True` / a dict, or a dict is given for a field without a model.
    """
    return _masked_model(model, _freeze(mask, _ROOT_PATH))


def _freeze(mask: object, path: str) -> FrozenMask:
    if not isinstance(mask, Mapping):
        expected = "a dict" if path == _ROOT_PATH else "`True` or a dict"
        raise TypeError(f"Validation mask at `{path}` must be {expected}, got `{type(mask).__name__}`.")
    items: list[tuple[str, bool | FrozenMask]] = []
    for key, value in cast("Mapping[object, object]", mask).items():
        if value is False:
            warnings.warn(
                f"`False` in the validation mask at `{path}.{key}` has no effect - fields not given are not validated.",
                stacklevel=2,
                skip_file_prefixes=_PACKAGE_FILES,
            )
        elif value is True:
            items.append((str(key), True))
        else:
            items.append((str(key), _freeze(value, f"{path}.{key}")))
    if not items:
        raise ValueError(
            f"Validation mask at `{path}` selects no fields - give `True` to validate the whole field, or its fields."
        )
    return tuple(sorted(items))


@lru_cache(maxsize=256)
def _masked_model(model: object, mask: FrozenMask) -> object:
    return _masked_type(model, mask, _ROOT_PATH)


def _masked_type(type_: object, mask: FrozenMask, path: str) -> object:
    """The type with its model reduced to the fields of the mask - through unions, lists and dicts of models."""
    if isinstance(type_, type) and issubclass(type_, msgspec.Struct):
        return _masked_struct(type_, mask, path)

    origin = get_origin(type_)
    args = get_args(type_)
    if origin is Annotated:
        return Annotated[_masked_type(args[0], mask, path), *type_.__metadata__]  # type: ignore[attr-defined]
    if origin in (Union, types.UnionType):
        values = [arg for arg in args if arg not in _NOT_VALUES]
        if len(values) != 1:  # e.g. a union of operations
            raise TypeError(
                f"Validation mask at `{path}`: the field is a union of models - give `True` to validate it."
            )
        masked = _masked_type(values[0], mask, path)
        return Union[tuple(masked if arg is values[0] else arg for arg in args)]  # noqa: UP007  # built from a tuple
    if origin in (list, set, frozenset) and len(args) == 1:
        return types.GenericAlias(origin, (_masked_type(args[0], mask, path),))
    if origin is tuple and len(args) == 2 and args[1] is Ellipsis:  # `tuple[item, ...]`
        return types.GenericAlias(tuple, (_masked_type(args[0], mask, path), Ellipsis))
    if origin is dict and len(args) == 2:  # `dict[key, value]`
        return types.GenericAlias(dict, (args[0], _masked_type(args[1], mask, path)))
    raise TypeError(f"Validation mask at `{path}`: the field has no model - give `True` to validate it.")


def _masked_struct(struct: type[msgspec.Struct], mask: FrozenMask, path: str) -> type[msgspec.Struct]:
    fields = {field.name: field for field in msgspec.structs.fields(struct)}
    unknown = [name for name, _ in mask if name not in fields]
    if unknown:
        raise ValueError(
            f"Validation mask at `{path}`: `{struct.__name__}` has no fields {unknown} - its fields: {sorted(fields)}."
        )

    masked_fields: list[tuple[str, type] | tuple[str, type, object]] = []
    for name, field_mask in mask:
        field = fields[name]
        masked = (
            field.type if not isinstance(field_mask, tuple) else _masked_type(field.type, field_mask, f"{path}.{name}")
        )
        field_type = cast("type", masked)  # any type form (unions, generics, ...) - msgspec annotates it as `type`
        if field.required:
            masked_fields.append((name, field_type))
        elif field.default_factory is not msgspec.NODEFAULT:
            masked_fields.append((name, field_type, msgspec.field(default_factory=field.default_factory)))
        else:
            masked_fields.append((name, field_type, field.default))

    config = struct.__struct_config__
    renamed = {name: fields[name].encode_name for name, _ in mask if fields[name].encode_name != name}
    return msgspec.defstruct(
        f"{struct.__name__}Masked",
        masked_fields,
        module=struct.__module__,
        kw_only=True,
        forbid_unknown_fields=False,  # fields not given in the mask are ignored
        rename=renamed or None,
        tag=config.tag,
        tag_field=config.tag_field,
    )
