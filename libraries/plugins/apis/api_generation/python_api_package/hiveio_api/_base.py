"""
Base class of all generated public models.

Models are plain frozen dataclasses holding builtin values exactly as they came in the JSON response.
Deserialization (`from_builtins`) never validates anything:

- fields present in the data but not declared in the model are kept and accessible as attributes
  (`model.some_new_field`), they are just unknown to type checkers / IDE,
- required fields missing in the data are set to `None` (optional ones get their default).

Both situations are reported with warnings (`UnexpectedFieldWarning`, `MissingFieldWarning`), which can be
disabled per call (`warn=False`) or with the standard `warnings` filters.

Validation of a response against Hive types is a separate, explicit step - see `hiveio_api.validate_schema`.
"""

from __future__ import annotations

import dataclasses
import json
import types
import warnings
from collections.abc import Callable, Mapping
from dataclasses import dataclass
from typing import Any, Self, Union, get_args, get_origin, get_type_hints

__all__ = [
    "HiveModel",
    "MissingFieldWarning",
    "UnexpectedFieldWarning",
]

_Converter = Callable[[Any, bool], Any]


class UnexpectedFieldWarning(UserWarning):
    """Data contains a field which is not declared in the model."""


class MissingFieldWarning(UserWarning):
    """Field declared in the model is missing in the data."""


@dataclass(frozen=True, kw_only=True)
class HiveModel:
    @classmethod
    def from_builtins(cls, data: Mapping[str, Any], *, warn: bool = True) -> Self:
        """
        Build the model from builtins (e.g. result of `json.loads`) without any validation.

        Args:
            data: Mapping with the model content.
            warn: Emit `UnexpectedFieldWarning` / `MissingFieldWarning` when the data does not match the model.

        Returns:
            Model instance. Nested models (also inside lists and dicts) are built the same way.
        """
        return _build(cls, data, warn)  # type: ignore[no-any-return]

    def json(self) -> str:
        """
        Serialize the model (including fields not declared in the model) to JSON.

        Optional fields holding `None` are omitted, so missing optional fields are not turned into `null`.
        """
        return json.dumps(_to_builtins(self))


@dataclass(frozen=True)
class _FieldPlan:
    attribute: str
    key: str
    converter: _Converter | None
    required: bool
    default: Any


@dataclass(frozen=True)
class _ModelPlan:
    fields: tuple[_FieldPlan, ...]
    attribute_by_key: Mapping[str, str]
    reserved: frozenset[str]

    @property
    def omitted_when_none(self) -> frozenset[str]:
        return frozenset(plan.attribute for plan in self.fields if not plan.required and plan.default is None)


def _build(model: type[Any], data: Mapping[str, Any], warn: bool) -> Any:
    plan = _model_plan(model)
    instance = object.__new__(model)
    present_keys = set(data)

    for field_plan in plan.fields:
        if field_plan.key in data:
            value = data[field_plan.key]
            if field_plan.converter is not None and value is not None:
                value = field_plan.converter(value, warn)
            present_keys.discard(field_plan.key)
        else:
            value = field_plan.default
            if warn and field_plan.required:
                warnings.warn(
                    f"{model.__qualname__}: field `{field_plan.key}` is missing in the data, set to None",
                    MissingFieldWarning,
                    stacklevel=3,
                )
        object.__setattr__(instance, field_plan.attribute, value)

    for key in present_keys:
        if warn:
            warnings.warn(
                f"{model.__qualname__}: field `{key}` is not declared in the model, available as attribute",
                UnexpectedFieldWarning,
                stacklevel=3,
            )
        attribute = f"{key}_" if key in plan.reserved else key
        object.__setattr__(instance, attribute, data[key])

    return instance


_model_plans: dict[type[Any], _ModelPlan] = {}


def _model_plan(model: type[Any]) -> _ModelPlan:
    """Plan of building the model, computed once per model class."""
    if (plan := _model_plans.get(model)) is None:
        plan = _model_plans[model] = _create_model_plan(model)
    return plan


def _create_model_plan(model: type[Any]) -> _ModelPlan:
    hints = get_type_hints(model)
    names = {field_.name for field_ in dataclasses.fields(model)}
    field_plans = tuple(
        _FieldPlan(
            attribute=field_.name,
            key=_key_for_attribute(field_.name, names),
            converter=_converter_for(hints[field_.name]),
            required=field_.default is dataclasses.MISSING and field_.default_factory is dataclasses.MISSING,
            default=None if field_.default is dataclasses.MISSING else field_.default,
        )
        for field_ in dataclasses.fields(model)
    )
    reserved = frozenset(name for name in dir(model) if not name.startswith("__")) | names
    return _ModelPlan(
        fields=field_plans,
        attribute_by_key={plan.key: plan.attribute for plan in field_plans},
        reserved=reserved,
    )


def _key_for_attribute(attribute: str, names: set[str]) -> str:
    """Generated models rename fields clashing with python keywords/attributes by appending `_` (`from` -> `from_`)."""
    if attribute.endswith("_") and attribute[:-1] and attribute[:-1] not in names:
        return attribute[:-1]
    return attribute


def _converter_for(annotation: Any) -> _Converter | None:
    """Return a function building nested models in a value of the given type, or None if nothing to build."""
    if isinstance(annotation, type) and dataclasses.is_dataclass(annotation):
        model = annotation
        return lambda value, warn: _build(model, value, warn) if isinstance(value, Mapping) else value

    origin = get_origin(annotation)
    args = get_args(annotation)

    if origin in (Union, types.UnionType):
        return _union_converter(args)

    if origin is list and args and (item_converter := _converter_for(args[0])) is not None:
        return lambda value, warn: [item_converter(i, warn) for i in value] if isinstance(value, list) else value

    if origin is dict and len(args) == 2 and (value_converter := _converter_for(args[1])) is not None:
        return lambda value, warn: (
            {k: value_converter(v, warn) for k, v in value.items()} if isinstance(value, dict) else value
        )

    return None


def _union_converter(members: tuple[Any, ...]) -> _Converter | None:
    models = [member for member in members if isinstance(member, type) and dataclasses.is_dataclass(member)]
    lists = [member for member in members if get_origin(member) is list and _converter_for(member) is not None]

    if not models and not lists:
        return None

    list_converter = _converter_for(lists[0]) if lists else None

    def convert(value: Any, warn: bool) -> Any:
        if isinstance(value, Mapping) and models:
            return _build(_best_matching_model(models, value), value, warn)
        if isinstance(value, list) and list_converter is not None:
            return list_converter(value, warn)
        return value

    return convert


def _best_matching_model(models: list[type[Any]], value: Mapping[str, Any]) -> type[Any]:
    """Pick the model whose declared keys overlap the data the most."""
    keys = set(value)
    return max(models, key=lambda model: len(keys & set(_model_plan(model).attribute_by_key)))


def _to_builtins(value: Any) -> Any:
    if dataclasses.is_dataclass(value) and not isinstance(value, type):
        plan = _model_plan(type(value))
        names = {field_plan.attribute for field_plan in plan.fields}
        omitted_when_none = plan.omitted_when_none
        return {
            _key_for_attribute(name, names): _to_builtins(item)
            for name, item in vars(value).items()
            if not (item is None and name in omitted_when_none)
        }
    if isinstance(value, list | tuple):
        return [_to_builtins(item) for item in value]
    if isinstance(value, dict):
        return {key: _to_builtins(item) for key, item in value.items()}
    return value
