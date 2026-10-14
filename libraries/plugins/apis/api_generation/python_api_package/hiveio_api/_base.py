"""
Base class of all generated public models.

Models are plain frozen dataclasses holding builtin values exactly as they came in the JSON response.
Deserialization (`from_builtins`) never validates anything:

- fields present in the data but not declared in the model are kept and accessible as attributes
  (`model.some_new_field`), they are just unknown to type checkers / IDE,
- required fields missing in the data are set to `None` (optional ones get their default).

Fields renamed in Python (e.g. `Block stats` -> `Block_stats`, `from` -> `from_`) keep their JSON key in
`field(metadata={"alias": ...})`.

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
from typing import (
    TYPE_CHECKING,
    Any,
    Final,
    Literal,
    Self,
    TypeVar,
    Union,
    cast,
    get_args,
    get_origin,
    get_type_hints,
    overload,
)

from hiveio_api._errors import UnsupportedTypeError

if TYPE_CHECKING:
    from _typeshed import DataclassInstance

__all__ = [
    "HiveModel",
    "MissingFieldWarning",
    "UnexpectedFieldWarning",
    "to_builtins",
    "value_builder",
]

T = TypeVar("T")
ModelT = TypeVar("ModelT", bound="DataclassInstance")

_Converter = Callable[[object, bool], object]


class UnexpectedFieldWarning(UserWarning):
    """Data contains a field which is not declared in the model."""


class MissingFieldWarning(UserWarning):
    """Field declared in the model is missing in the data."""


_ALIAS_METADATA_KEY: Final[str] = "alias"
_MISSING_KEYS_ATTRIBUTE: Final[str] = "_hive_model_missing_keys"
"""Holds JSON keys of declared fields which were missing in the data the model was built from."""


@dataclass(frozen=True, kw_only=True)
class HiveModel:
    """
    Base class of generated public API models - frozen dataclasses built from builtins without validation.

    Use `from_builtins` to build a model from parsed JSON and `json` to serialize it back.
    """

    @classmethod
    def from_builtins(cls, data: Mapping[str, object], *, warn: bool = True) -> Self:
        """
        Build the model from builtins (e.g. result of `json.loads`) without any validation.

        Args:
            data: Mapping with the model content.
            warn: Emit `UnexpectedFieldWarning` / `MissingFieldWarning` when the data does not match the model.

        Returns:
            Model instance. Nested models (also inside lists and dicts) are built the same way.
        """
        return _build(cls, data, warn)

    def json(self) -> str:
        """
        Serialize the model (including fields not declared in the model) to JSON.

        A model built by `from_builtins` is serialized with exactly the keys present in the source data (fields
        missing in the data are omitted, explicit `null` values are kept), so validation of the JSON gives the same
        result as validation of the source data. For models created directly, optional fields holding `None` are
        omitted.
        """
        return json.dumps(to_builtins(self))


@dataclass(frozen=True)
class _FieldPlan:
    attribute: str
    key: str
    converter: _Converter | None
    required: bool
    default: object


@dataclass(frozen=True)
class _ModelPlan:
    fields: tuple[_FieldPlan, ...]
    attribute_by_key: Mapping[str, str]
    key_by_attribute: Mapping[str, str]
    reserved: frozenset[str]
    omitted_when_none: frozenset[str]


def _build(model: type[ModelT], data: Mapping[str, object], warn: bool) -> ModelT:
    plan = _model_plan(model)
    instance = object.__new__(model)
    present_keys = set(data)
    missing_keys: list[str] = []

    for field_plan in plan.fields:
        if field_plan.key in data:
            value = data[field_plan.key]
            if field_plan.converter is not None and value is not None:
                value = field_plan.converter(value, warn)
            present_keys.discard(field_plan.key)
        else:
            value = field_plan.default
            missing_keys.append(field_plan.key)
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

    object.__setattr__(instance, _MISSING_KEYS_ATTRIBUTE, frozenset(missing_keys))
    return instance


_model_plans: dict[type[DataclassInstance], _ModelPlan] = {}


def _model_plan(model: type[DataclassInstance]) -> _ModelPlan:
    """Plan of building the model, computed once per model class."""
    if (plan := _model_plans.get(model)) is None:
        plan = _model_plans[model] = _create_model_plan(model)
    return plan


def _create_model_plan(model: type[DataclassInstance]) -> _ModelPlan:
    hints = get_type_hints(model)
    field_plans = tuple(
        _FieldPlan(
            attribute=field_.name,
            key=field_.metadata.get(_ALIAS_METADATA_KEY, field_.name),
            converter=_converter_for(hints[field_.name]),
            required=field_.default is dataclasses.MISSING and field_.default_factory is dataclasses.MISSING,
            default=None if field_.default is dataclasses.MISSING else field_.default,
        )
        for field_ in dataclasses.fields(model)
    )
    names = {plan.attribute for plan in field_plans}
    reserved = frozenset(name for name in dir(model) if not name.startswith("__")) | names
    return _ModelPlan(
        fields=field_plans,
        attribute_by_key={plan.key: plan.attribute for plan in field_plans},
        key_by_attribute={plan.attribute: plan.key for plan in field_plans},
        reserved=reserved,
        omitted_when_none=frozenset(
            plan.attribute for plan in field_plans if not plan.required and plan.default is None
        ),
    )


def _key_of_undeclared_attribute(attribute: str, plan: _ModelPlan) -> str:
    """Undeclared keys clashing with model attributes are stored with `_` appended (see `_build`)."""
    if attribute.endswith("_") and attribute[:-1] in plan.reserved:
        return attribute[:-1]
    return attribute


_value_builders: dict[object, Callable[[object], object]] = {}
# `Any` is a value here, not an annotation - it is used by models generated by hiveio-api-client-generator
_BUILTIN_TYPES: Final[frozenset[object]] = frozenset({str, int, float, bool, type(None), None, object, Any, list, dict})
_UNION_TYPES: Final[frozenset[object]] = frozenset({Union, types.UnionType})


def value_builder(annotation: type[T]) -> Callable[[object], T]:
    """
    Return a function building a value of the given type from builtins (e.g. result of `json.loads`).

    Nested public models (also inside lists, dicts and unions) are built with `from_builtins`, everything else is left
    as it is - nothing is validated. Values of types without public models are returned without being traversed.
    Builders are cached per type.

    Raises:
        UnsupportedTypeError: When the type consists of something else than builtins and dataclasses (e.g. msgspec
            models or Hive types from `schemas.fields`) - such types can be used only for validation.
    """
    if (builder := _value_builders.get(annotation)) is None:
        converter = _converter_for(annotation)
        builder = _identity if converter is None else lambda value: converter(value, True)
        _value_builders[annotation] = builder
    return cast("Callable[[object], T]", builder)  # cached per type - builds values of `annotation` (not validated)


def _identity(value: object) -> object:
    return value


def _converter_for(annotation: object) -> _Converter | None:
    """Return a function building nested models in a value of the given type, or None if nothing to build."""
    if annotation in _BUILTIN_TYPES:
        return None

    if isinstance(annotation, type) and dataclasses.is_dataclass(annotation):
        model = annotation
        return lambda value, warn: _build(model, value, warn) if isinstance(value, Mapping) else value

    origin = get_origin(annotation)
    args = get_args(annotation)

    if origin is Literal:
        return None

    if origin in _UNION_TYPES:
        return _union_converter(args)

    if origin is list and len(args) == 1:
        if (item_converter := _converter_for(args[0])) is None:
            return None
        return lambda value, warn: [item_converter(i, warn) for i in value] if isinstance(value, list) else value

    if origin is dict and len(args) == 2:
        if (value_converter := _converter_for(args[1])) is None:
            return None
        return lambda value, warn: (
            {k: value_converter(v, warn) for k, v in value.items()} if isinstance(value, dict) else value
        )

    if origin is tuple and args and args[-1] is not Ellipsis:
        return _tuple_converter(args)

    raise UnsupportedTypeError(
        f"`{annotation}` cannot be built from builtins without validation - only builtins and dataclasses (public "
        "models), also in lists, dicts, tuples and unions, are supported."
    )


def _tuple_converter(items: tuple[object, ...]) -> _Converter | None:
    """A tuple of fixed length is built from a JSON array, item by item."""
    converters = [_converter_for(item) for item in items]
    if all(converter is None for converter in converters):
        return None

    def convert(value: object, warn: bool) -> object:
        if not isinstance(value, list) or len(value) != len(converters):
            return value
        return tuple(
            item if converter is None or item is None else converter(item, warn)
            for item, converter in zip(value, converters, strict=True)
        )

    return convert


def _union_converter(members: tuple[object, ...]) -> _Converter | None:
    """
    Without validation a member is chosen by the JSON kind of the value - an object or an array.

    Several members of the same kind are allowed only when they are tagged: models by a field of the same name with a
    distinct `Literal` value in each of them (e.g. `{"type": "transfer_operation", ...}`), tuples by a distinct
    `Literal` first item (e.g. `["transfer", {...}]`).
    """
    if (tagged := _tagged_union_converter(members)) is not None:
        return tagged

    converters = {member: _converter_for(member) for member in members}
    models = [member for member in members if isinstance(member, type) and dataclasses.is_dataclass(member)]
    dicts = [member for member in members if get_origin(member) is dict and converters[member] is not None]
    lists = [member for member in members if get_origin(member) in {list, tuple} and converters[member] is not None]
    if (models and dicts) or len(dicts) > 1 or len(lists) > 1:
        raise UnsupportedTypeError(
            f"Union of `{members}` cannot be built without validation - more than one of its members is built from "
            "JSON objects or arrays."
        )

    if not models and not dicts and not lists:
        return None

    dict_converter = converters[dicts[0]] if dicts else None
    list_converter = converters[lists[0]] if lists else None

    def convert(value: object, warn: bool) -> object:
        if isinstance(value, Mapping):
            if models:
                return _build(_best_matching_model(models, value), value, warn)
            if dict_converter is not None:
                return dict_converter(value, warn)
        if isinstance(value, list) and list_converter is not None:
            return list_converter(value, warn)
        return value

    return convert


def _tagged_union_converter(members: tuple[object, ...]) -> _Converter | None:
    tagged = [member for member in members if member is not type(None)]
    if len(tagged) < 2:
        return None

    models = [member for member in tagged if isinstance(member, type) and dataclasses.is_dataclass(member)]
    if len(models) == len(tagged):
        tags = _model_tags(models)
        if tags is None:
            return None
        key, model_by_tag = tags
        return lambda value, warn: (
            _build(model_by_tag[value[key]], value, warn)
            if isinstance(value, Mapping) and value.get(key) in model_by_tag
            else value
        )

    if all(get_origin(member) is tuple for member in tagged):
        tuple_by_tag: dict[object, _Converter | None] = {}
        for member in tagged:
            first = get_args(member)[0]
            if get_origin(first) is not Literal or len(get_args(first)) != 1 or get_args(first)[0] in tuple_by_tag:
                return None
            tuple_by_tag[get_args(first)[0]] = _tuple_converter(get_args(member))
        return lambda value, warn: (
            converter(value, warn)
            if isinstance(value, list) and value and (converter := tuple_by_tag.get(value[0])) is not None
            else value
        )

    return None


def _model_tags(models: list[type[DataclassInstance]]) -> tuple[str, dict[object, type[DataclassInstance]]] | None:
    """The JSON key and models by its `Literal` value, when every model has a distinct one under the same key."""
    for key in _model_plan(models[0]).attribute_by_key:
        model_by_tag: dict[object, type[DataclassInstance]] = {}
        for model in models:
            attribute = _model_plan(model).attribute_by_key.get(key)
            annotation = get_type_hints(model).get(attribute) if attribute is not None else None
            if get_origin(annotation) is not Literal or len(get_args(annotation)) != 1:
                break
            model_by_tag[get_args(annotation)[0]] = model
        else:
            if len(model_by_tag) == len(models):
                return key, model_by_tag
    return None


def _best_matching_model(models: list[type[DataclassInstance]], value: Mapping[str, object]) -> type[DataclassInstance]:
    """Pick the model whose declared keys overlap the data the most."""
    keys = set(value)
    return max(models, key=lambda model: len(keys & set(_model_plan(model).attribute_by_key)))


@overload
def to_builtins(value: DataclassInstance) -> dict[str, object]: ...


@overload
def to_builtins(value: object) -> object: ...


def to_builtins(value: object) -> object:
    """Convert public models (also nested in lists, tuples and dicts) back to builtins, keys as in `HiveModel.json`."""
    if dataclasses.is_dataclass(value) and not isinstance(value, type):
        return _model_to_builtins(value)
    if isinstance(value, list | tuple):
        return [to_builtins(item) for item in value]
    if isinstance(value, dict):
        return {key: to_builtins(item) for key, item in value.items()}
    return value


def _model_to_builtins(model: DataclassInstance) -> dict[str, object]:
    plan = _model_plan(type(model))
    missing_keys: frozenset[str] | None = getattr(model, _MISSING_KEYS_ATTRIBUTE, None)
    result: dict[str, object] = {}
    for attribute, item in vars(model).items():
        if attribute == _MISSING_KEYS_ATTRIBUTE:
            continue
        key = plan.key_by_attribute.get(attribute)
        if key is None:
            result[_key_of_undeclared_attribute(attribute, plan)] = to_builtins(item)
            continue
        omitted = (
            key in missing_keys if missing_keys is not None else item is None and attribute in plan.omitted_when_none
        )
        if not omitted:
            result[key] = to_builtins(item)
    return result
