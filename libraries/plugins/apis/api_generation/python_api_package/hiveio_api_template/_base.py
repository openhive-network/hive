"""
Base class of all generated public models.

Models are plain dataclasses holding builtin values exactly as they came in the JSON response.
Deserialization (`from_builtins`) never validates anything:

- fields present in the data but not declared in the model are kept and accessible as attributes
  (`model.some_new_field`), they are just unknown to type checkers / IDE,
- required fields missing in the data are set to `None` (optional ones get their default).

Fields renamed in Python (e.g. `Block stats` -> `Block_stats`, `from` -> `from_`) keep their JSON key in
`field(metadata={"alias": ...})`.

Both situations are reported with warnings (`UnexpectedFieldWarning`, `MissingFieldWarning`), which can be
disabled per call (`warn=False`) or with the standard `warnings` filters.

Types which build themselves from builtins (`from_builtins` / `to_builtins`, e.g. `schemas.Asset` of assets,
`schemas.HiveDateTime` of dates) are built by their `from_builtins` - a value they reject is kept as it is and reported
with `InvalidValueWarning`. Integers of 64 bits (`Int64t`, `Uint64t`, `HiveInt`) sent by hived as strings (the ones not
fitting doubles of JSON) are built as `int` and converted back to strings, as hived sends them.

Validation of a response against Hive types is a separate, explicit step - see `hiveio_api.validate_schema`.
"""

from __future__ import annotations

import builtins
import dataclasses
import json
import os
import re
import types
import warnings
from abc import ABC, abstractmethod
from collections.abc import Callable, Mapping
from copy import deepcopy
from dataclasses import dataclass
from enum import Enum
from typing import (
    TYPE_CHECKING,
    Any,
    ClassVar,
    Final,
    Literal,
    Protocol,
    Self,
    TypeGuard,
    TypeVar,
    Union,
    cast,
    get_args,
    get_origin,
    get_type_hints,
    overload,
)

from hiveio_api._errors import UnsupportedTypeError
from schemas.fields import public
from schemas.fields.public import Asset

if TYPE_CHECKING:
    from _typeshed import DataclassInstance

__all__ = [
    "HiveModel",
    "InvalidValueWarning",
    "MissingFieldWarning",
    "OperationBase",
    "UnexpectedFieldWarning",
    "VirtualOperationBase",
    "to_api_builtins",
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


class InvalidValueWarning(UserWarning):
    """Value cannot be built as its type (e.g. `Asset` from a malformed asset) - it is kept as it is."""


class _BuiltinsConvertible(Protocol):
    """Type building itself from builtins and converting itself back (e.g. `schemas.Asset`)."""

    @classmethod
    def from_builtins(cls, value: object) -> Self: ...

    def to_builtins(self) -> object: ...


class _AssetForm(Enum):
    """Form of assets in a field, told by the name of its type (`LegacyAssetHive` - legacy, `AssetHive` - NAI)."""

    NAI = "nai"
    LEGACY = "legacy"


_ASSET_ALIASES: Final[frozenset[str]] = frozenset(
    name for name in public.__all__ if getattr(public, name) is Asset and name != Asset.__name__
)
"""Aliases of assets telling the form of their fields (`AssetHive`, `LegacyAnyAsset`, ...) - not `Asset` itself."""
_INTEGER_64_ALIASES: Final[frozenset[str]] = frozenset({"Int64t", "Uint64t", "HiveInt"})
"""Aliases of integers of 64 bits - `int`, sent by hived as strings when they do not fit doubles of JSON."""
_MAX_SAFE_INTEGER: Final[int] = 2**53 - 1
"""Integers of greater magnitude are sent as strings by hived (fc) - they do not fit doubles of JSON."""
_WORD: Final = re.compile(r"\w+")


_ALIAS_METADATA_KEY: Final[str] = "alias"
_MISSING_KEYS_ATTRIBUTE: Final[str] = "_hive_model_missing_keys"
"""Holds JSON keys of declared fields which were missing in the data the model was built from."""
_RENAMED_KEYS_ATTRIBUTE: Final[str] = "_hive_model_renamed_keys"
"""Keys of undeclared fields stored under other attributes (clashing with attributes of the model or each other)."""
_INTERNAL_ATTRIBUTES: Final[frozenset[str]] = frozenset({_MISSING_KEYS_ATTRIBUTE, _RENAMED_KEYS_ATTRIBUTE})
_PACKAGE_FILES: Final[tuple[str, ...]] = (os.path.dirname(__file__),)
"""Warnings about data point at the first caller outside the package (models are built recursively)."""


@dataclass(kw_only=True)
class HiveModel:
    """
    Base class of generated public API models - dataclasses built from builtins without validation.

    Use `from_builtins` (or `parse_raw` for JSON text) to build a model from parsed JSON and `to_builtins` (`dict`, or
    `json`) to convert it back. Fields are attributes (`model.from_`), also available by keys of JSON (`model["from"]`).
    """

    __iter__ = None  # fields are not iterated by `__getitem__` with indexes (the sequence protocol)

    def __getitem__(self, key: str) -> object:
        """
        Value of the field given by its key in JSON (e.g. `model["from"]`) or by its attribute (`model["from_"]`).

        Fields not declared in the model (kept from the data it was built from) are available the same way.

        Raises:
            KeyError: When the model has no such field.
        """
        return vars(self)[self.__attribute_of(key)]

    def __setitem__(self, key: str, value: object) -> None:
        """
        Set the field given by its key in JSON (e.g. `model["from"] = "bob"`) or by its attribute (`model["from_"]`).

        Only existing fields can be set - declared ones and undeclared ones kept from the data the model was built from.

        Raises:
            KeyError: When the model has no such field.
        """
        setattr(self, self.__attribute_of(key), value)

    def __attribute_of(self, key: str) -> str:
        """Attribute holding the field given by its key in JSON or by its attribute (as `__getitem__`)."""
        values = vars(self)
        for attribute, renamed_key in values.get(_RENAMED_KEYS_ATTRIBUTE, {}).items():  # undeclared, stored elsewhere
            if renamed_key == key:
                return attribute
        attribute = _model_plan(type(self)).attribute_by_key.get(key, key)
        if attribute in values and attribute not in _INTERNAL_ATTRIBUTES:
            return attribute
        raise KeyError(key)

    def __contains__(self, key: object) -> bool:
        """Whether the model has the field given by its key in JSON or by its attribute (as `__getitem__`)."""
        if not isinstance(key, str):
            return False
        try:
            self[key]
        except KeyError:
            return False
        return True

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

    @classmethod
    def parse_raw(cls, raw: str | bytes, *, warn: bool = True) -> Self:
        """
        Build the model from its JSON (e.g. of `json()`) without any validation - as `from_builtins`.

        Args:
            raw: JSON of an object.
            warn: As in `from_builtins`.

        Raises:
            json.JSONDecodeError: When the text is not JSON.
            TypeError: When the JSON is not an object.
        """
        data = json.loads(raw)
        if not isinstance(data, builtins.dict):
            raise TypeError(f"JSON of {cls.__qualname__} has to be an object, not {type(data).__name__}")
        return cls.from_builtins(data, warn=warn)

    def to_builtins(self) -> builtins.dict[str, object]:
        """
        Convert the model (including fields not declared in the model) back to JSON builtins, with keys of the API.

        A model built by `from_builtins` keeps exactly the keys present in the source data (fields missing in the data
        are omitted unless set since, explicit `null` values are kept), so validation of the result gives the same
        result as validation of the source data. For models created directly, optional fields holding `None` are
        omitted. Assets take the form of their fields (`LegacyAssetHive`: `"1.000 HIVE"`, `AssetHive`: NAI), assets
        outside fields of assets keep the form they were created in.
        """
        return _model_to_builtins(self)

    def dict(self) -> builtins.dict[str, object]:
        """The model as JSON builtins - the same as `to_builtins`."""
        return self.to_builtins()

    def json(
        self,
        *,
        indent: int | str | None = None,
        separators: tuple[str, str] | None = None,
        ensure_ascii: bool = False,
        order: Literal["deterministic", "sorted"] | None = None,
    ) -> str:
        """
        Serialize the model to JSON - `to_builtins` given to `json.dumps`.

        Args:
            indent: Indentation of nested objects and arrays, as in `json.dumps` (compact single line by default).
            separators: Separators of items and of keys and values, as in `json.dumps`.
            ensure_ascii: Escape non-ASCII characters (`\\uXXXX`) - UTF-8 by default, as models of schemas before.
            order: Order of keys of objects - `"sorted"` (or `"deterministic"`) sorts them, as models of schemas before;
                the order of the fields by default.
        """
        return json.dumps(
            self.to_builtins(),
            indent=indent,
            separators=separators,
            ensure_ascii=ensure_ascii,
            sort_keys=order is not None,
        )

    def copy(self, exclude: set[str] | None = None) -> Self:
        """A deep copy of the model - with the given fields (attributes) set to `None`, as models of schemas before."""
        copied = deepcopy(self)
        for attribute in exclude or ():
            setattr(copied, attribute, None)
        return copied


@dataclass(kw_only=True)
class OperationBase(HiveModel, ABC):
    """
    Base class of values of operations (`hiveio_api.operations`, e.g. `TransferOperation`).

    Values of operations are generated from the description of operations of hived - each of them tells its number and
    its names.

    Attributes:
        operation_id: Number of the operation - its position in `hive::protocol::operation`
            (`libraries/protocol/include/hive/protocol/operations.hpp`), e.g. the bit of the operation in binary
            filters (`hiveio_api.operations.binary_filters`).
        virtual: Whether the operation is virtual - created by hived, never sent in transactions.
    """

    operation_id: ClassVar[int]
    virtual: ClassVar[bool]

    @classmethod
    @abstractmethod
    def get_name(cls) -> str:
        """Name of the operation, as in the legacy form (e.g. `transfer`)."""

    @classmethod
    @abstractmethod
    def get_name_with_suffix(cls) -> str:
        """Name of the operation with the `_operation` suffix, as in the HF26 form (e.g. `transfer_operation`)."""


@dataclass(kw_only=True)
class VirtualOperationBase(OperationBase):
    """Base class of values of virtual operations - created by hived, e.g. `FillConvertRequestOperation`."""


@dataclass(frozen=True)
class _FieldPlan:
    attribute: str
    key: str
    converter: _Converter | None
    required: bool
    default: object
    default_factory: Callable[[], object] | None
    asset_form: _AssetForm | None

    def default_value(self) -> object:
        """Value of the field when its key is missing in the data."""
        return self.default if self.default_factory is None else self.default_factory()

    def holds_default(self, value: object) -> bool:
        """Whether the value is still the one `default_value` gave (not set by the user since)."""
        return value is self.default if self.default_factory is None else value == self.default_factory()


@dataclass(frozen=True)
class _ModelPlan:
    fields: tuple[_FieldPlan, ...]
    attribute_by_key: Mapping[str, str]
    key_by_attribute: Mapping[str, str]
    reserved: frozenset[str]
    omitted_when_none: frozenset[str]
    field_by_attribute: Mapping[str, _FieldPlan]


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
            value = field_plan.default_value()
            missing_keys.append(field_plan.key)
            if warn and field_plan.required:
                warnings.warn(
                    f"{model.__qualname__}: field `{field_plan.key}` is missing in the data, set to None",
                    MissingFieldWarning,
                    skip_file_prefixes=_PACKAGE_FILES,
                )
        object.__setattr__(instance, field_plan.attribute, value)

    renamed: dict[str, str] = {}
    for key in [key for key in data if key in present_keys] if present_keys else ():  # in the order of the data
        if warn:
            warnings.warn(
                f"{model.__qualname__}: field `{key}` is not declared in the model, available as attribute",
                UnexpectedFieldWarning,
                skip_file_prefixes=_PACKAGE_FILES,
            )
        attribute = key
        while attribute in plan.reserved or attribute in instance.__dict__:  # clashes - `json` is kept as `json_`
            attribute += "_"
        if attribute != key:
            renamed[attribute] = key
        object.__setattr__(instance, attribute, data[key])
    if renamed:
        object.__setattr__(instance, _RENAMED_KEYS_ATTRIBUTE, renamed)

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
            converter=_converter_for(hints[field_.name], integers_from_strings=_has_integer_64(field_.type)),
            required=field_.default is dataclasses.MISSING and field_.default_factory is dataclasses.MISSING,
            default=None if field_.default is dataclasses.MISSING else field_.default,
            default_factory=None if field_.default_factory is dataclasses.MISSING else field_.default_factory,
            asset_form=_asset_form(field_.type),
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
        field_by_attribute={plan.attribute: plan for plan in field_plans},
    )


def _asset_form(annotation: object) -> _AssetForm | None:
    """
    Form of assets told by the source text of the annotation (models use postponed annotations).

    All aliases of assets resolve to `Asset`, only their names tell the form - e.g. `list[LegacyAnyAsset] | None`.
    """
    if not isinstance(annotation, str):
        return None
    names = set(_WORD.findall(annotation)) & _ASSET_ALIASES
    if not names:
        return None
    return _AssetForm.LEGACY if all(name.startswith("Legacy") for name in names) else _AssetForm.NAI


def _has_integer_64(annotation: object) -> bool:
    """Whether the source text of the annotation names integers of 64 bits (their aliases resolve to `int`)."""
    return isinstance(annotation, str) and not _INTEGER_64_ALIASES.isdisjoint(_WORD.findall(annotation))


def _key_of_undeclared_attribute(attribute: str, renamed: Mapping[str, str]) -> str:
    """Key of an undeclared field - stored under another attribute when it clashes (see `_build`)."""
    return renamed.get(attribute, attribute)


_value_builders: dict[object, Callable[[object], object]] = {}
# `Any` is a value here, not an annotation - it is used by models generated by hiveio-api-client-generator
_BUILTIN_TYPES: Final[frozenset[object]] = frozenset({str, int, float, bool, type(None), None, object, Any, list, dict})
_UNION_TYPES: Final[frozenset[object]] = frozenset({Union, types.UnionType})
_PLAIN_TYPES: Final[frozenset[type]] = frozenset({str, int, float, bool, type(None)})
"""Types of plain JSON values (other than arrays and objects)."""


def _is_mapping(value: object) -> TypeGuard[Mapping[str, object]]:
    """Whether the value is a JSON object - `dict` checked first, the `Mapping` ABC check is slow."""
    return type(value) is dict or isinstance(value, Mapping)


def value_builder(annotation: type[T]) -> Callable[[object], T]:
    """
    Return a function building a value of the given type from builtins (e.g. result of `json.loads`).

    Nested public models (also inside lists, dicts and unions) are built with `from_builtins`, as types building
    themselves from builtins (e.g. `schemas.Asset`), everything else is left as it is - nothing is validated. Values of
    types without public models are returned without being traversed. Builders are cached per type.

    Raises:
        UnsupportedTypeError: When the type consists of something else than builtins, dataclasses and types building
            themselves from builtins (e.g. msgspec models or validating Hive types from `schemas.fields`) - such types
            can be used only for validation.
    """
    if (builder := _value_builders.get(annotation)) is None:
        converter = _converter_for(annotation)
        builder = _identity if converter is None else lambda value: converter(value, True)
        _value_builders[annotation] = builder
    return cast("Callable[[object], T]", builder)  # cached per type - builds values of `annotation` (not validated)


def build(annotation: type[T], data: object) -> T:
    """
    Build a value of the given type from builtins (e.g. result of `json.loads`), without validation.

    The type can be a public model, a union of them (e.g. `hiveio_api.operations.LegacyOperation`) or a container of
    them - as described in `value_builder`.
    """
    return value_builder(annotation)(data)


def _identity(value: object) -> object:
    return value


def _converter_for(annotation: object, *, integers_from_strings: bool = False) -> _Converter | None:
    """
    Return a function building nested models in a value of the given type, or None if nothing to build.

    Args:
        annotation: The type of the value.
        integers_from_strings: Build `int` of the type also from strings - integers of 64 bits (`_has_integer_64`).
    """
    if annotation is int and integers_from_strings:
        return _integer_from_string

    if annotation in _BUILTIN_TYPES:
        return None

    if _is_convertible_type(annotation):
        return _convertible_converter(annotation)

    if isinstance(annotation, type) and dataclasses.is_dataclass(annotation):
        model = annotation
        return lambda value, warn: _build(model, value, warn) if _is_mapping(value) else value

    origin = get_origin(annotation)
    args = get_args(annotation)

    if origin is Literal:
        return None

    if origin in _UNION_TYPES:
        return _union_converter(args, integers_from_strings=integers_from_strings)

    if origin is list and len(args) == 1:
        if (item_converter := _converter_for(args[0], integers_from_strings=integers_from_strings)) is None:
            return None
        return lambda value, warn: [item_converter(i, warn) for i in value] if isinstance(value, list) else value

    if origin is dict and len(args) == 2:
        if (value_converter := _converter_for(args[1], integers_from_strings=integers_from_strings)) is None:
            return None
        return lambda value, warn: (
            {k: value_converter(v, warn) for k, v in value.items()} if isinstance(value, dict) else value
        )

    if origin is tuple and args and args[-1] is not Ellipsis:
        return _tuple_converter(args, integers_from_strings=integers_from_strings)

    raise UnsupportedTypeError(
        f"`{annotation}` cannot be built from builtins without validation - only builtins, dataclasses (public "
        "models) and types building themselves from builtins (e.g. `schemas.Asset`), also in lists, dicts, tuples and "
        "unions, are supported."
    )


def _is_convertible_type(annotation: object) -> TypeGuard[type[_BuiltinsConvertible]]:
    return (
        isinstance(annotation, type)
        and not dataclasses.is_dataclass(annotation)
        and callable(getattr(annotation, "from_builtins", None))
        and callable(getattr(annotation, "to_builtins", None))
    )


def _convertible_converter(convertible: type[_BuiltinsConvertible]) -> _Converter:
    def convert(value: object, warn: bool) -> object:
        if type(value) not in _PLAIN_TYPES and isinstance(value, convertible):  # plain JSON values skip `isinstance`
            return value
        try:
            return convertible.from_builtins(value)
        except (TypeError, ValueError) as error:
            if warn:
                warnings.warn(
                    f"`{value!r}` cannot be built as {convertible.__qualname__}, kept as it is: {error}",
                    InvalidValueWarning,
                    skip_file_prefixes=_PACKAGE_FILES,
                )
            return value

    return convert


def _integer_from_string(value: object, warn: bool) -> object:
    """An integer of 64 bits - sent as a string when it does not fit doubles of JSON (`_MAX_SAFE_INTEGER`)."""
    if type(value) is not str:
        return value
    try:
        return int(value)
    except ValueError as error:
        if warn:
            warnings.warn(
                f"`{value!r}` cannot be built as int, kept as it is: {error}",
                InvalidValueWarning,
                skip_file_prefixes=_PACKAGE_FILES,
            )
        return value


def _tuple_converter(items: tuple[object, ...], *, integers_from_strings: bool = False) -> _Converter | None:
    """A tuple of fixed length is built from a JSON array, item by item."""
    converters = [_converter_for(item, integers_from_strings=integers_from_strings) for item in items]
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


def _union_converter(members: tuple[object, ...], *, integers_from_strings: bool = False) -> _Converter | None:
    """
    Without validation a member is chosen by the JSON kind of the value - an object, an array or a string of an integer.

    Several members of the same kind are allowed only when they are tagged: models by a field of the same name with a
    distinct `Literal` value in each of them (e.g. `{"type": "transfer_operation", ...}`), tuples by a distinct
    `Literal` first item (e.g. `["transfer", {...}]`).
    """
    if (tagged := _tagged_union_converter(members)) is not None:
        return tagged

    convertibles = [member for member in members if _is_convertible_type(member)]
    if convertibles:
        if len(convertibles) > 1 or any(m is not type(None) and m not in convertibles for m in members):
            raise UnsupportedTypeError(
                f"Union of `{members}` cannot be built without validation - a type building itself from builtins can "
                "be only in a union with None."
            )
        return _convertible_converter(convertibles[0])

    converters = {member: _converter_for(member, integers_from_strings=integers_from_strings) for member in members}
    models = [member for member in members if isinstance(member, type) and dataclasses.is_dataclass(member)]
    dicts = [member for member in members if get_origin(member) is dict and converters[member] is not None]
    lists = [member for member in members if get_origin(member) in {list, tuple} and converters[member] is not None]
    if (models and dicts) or len(dicts) > 1 or len(lists) > 1:
        raise UnsupportedTypeError(
            f"Union of `{members}` cannot be built without validation - more than one of its members is built from "
            "JSON objects or arrays."
        )

    integer_converter = converters.get(int) if str not in members else None  # a string of a `str` member is kept
    if not models and not dicts and not lists:
        return integer_converter

    dict_converter = converters[dicts[0]] if dicts else None
    list_converter = converters[lists[0]] if lists else None

    def convert(value: object, warn: bool) -> object:
        if _is_mapping(value):
            if models:
                return _build(_best_matching_model(models, value), value, warn)
            if dict_converter is not None:
                return dict_converter(value, warn)
        if isinstance(value, list) and list_converter is not None:
            return list_converter(value, warn)
        if integer_converter is not None:
            return integer_converter(value, warn)
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
            if _is_mapping(value) and value.get(key) in model_by_tag
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
    """Convert public models (also nested in lists, tuples and dicts) back to builtins, as `HiveModel.to_builtins`."""
    value_type = type(value)
    if value_type in _PLAIN_TYPES:  # most of the values - checked first
        if value_type is int and not -_MAX_SAFE_INTEGER <= cast("int", value) <= _MAX_SAFE_INTEGER:
            return str(value)  # as hived sends integers not fitting doubles of JSON
        return value
    if value_type is list or value_type is tuple:
        return [to_builtins(item) for item in cast("list[object]", value)]
    if value_type is dict:
        return {key: to_builtins(item) for key, item in cast("dict[object, object]", value).items()}
    if dataclasses.is_dataclass(value) and not isinstance(value, type):
        return _model_to_builtins(value)
    if _converts_itself(value_type):
        return cast("_BuiltinsConvertible", value).to_builtins()
    if isinstance(value, list | tuple):
        return [to_builtins(item) for item in value]
    if isinstance(value, dict):
        return {key: to_builtins(item) for key, item in value.items()}
    return value


_types_converting_themselves: dict[type, bool] = {}


def _converts_itself(value_type: type) -> bool:
    """Whether values of the type convert themselves to builtins (`to_builtins`) - computed once per type."""
    converts = _types_converting_themselves.get(value_type)
    if converts is None:
        converts = _types_converting_themselves[value_type] = _is_convertible_type(value_type)
    return converts


def _model_to_builtins(model: DataclassInstance) -> dict[str, object]:
    return {
        key: to_builtins(item)
        if field_plan is None or field_plan.asset_form is None
        else _to_api_builtins(item, field_plan.asset_form, None)
        for key, field_plan, item in _model_items(model)
    }


def _model_items(model: DataclassInstance) -> list[tuple[str, _FieldPlan | None, object]]:
    """JSON keys, plans of fields (None for undeclared ones) and values of the model, in the order of attributes."""
    plan = _model_plan(type(model))
    missing_keys: frozenset[str] | None = getattr(model, _MISSING_KEYS_ATTRIBUTE, None)
    renamed: Mapping[str, str] = getattr(model, _RENAMED_KEYS_ATTRIBUTE, {})
    items: list[tuple[str, _FieldPlan | None, object]] = []
    for attribute, item in vars(model).items():
        if attribute in _INTERNAL_ATTRIBUTES:
            continue
        field_plan = plan.field_by_attribute.get(attribute)
        if field_plan is None:
            items.append((_key_of_undeclared_attribute(attribute, renamed), None, item))
            continue
        omitted = (
            field_plan.key in missing_keys and field_plan.holds_default(item)  # set by the user after building - kept
            if missing_keys is not None
            else item is None and attribute in plan.omitted_when_none
        )
        if not omitted:
            items.append((field_plan.key, field_plan, item))
    return items


def to_api_builtins(value: object, *, testnet: Callable[[], bool]) -> object:
    """
    Convert parameters of an API call to builtins - as `to_builtins`, with legacy assets in symbols of the network.

    Assets take the form of their fields, as in `to_builtins`: an asset in a field of a legacy asset (e.g.
    `LegacyAssetHive` of operations of a `TransactionLegacy`) is sent in the legacy form, in a field of a NAI asset
    (e.g. `AssetHive`) in the NAI form, whatever form it was created in. Assets outside such fields keep their form.

    Args:
        value: Parameters - public models, assets and builtins, also nested in lists, tuples and dicts.
        testnet: Tells the network of the receiver - called only for assets sent in the legacy form (`TESTS` / `TBD`
            on the testnet, `HIVE` / `HBD` otherwise).
    """
    return _to_api_builtins(value, None, testnet)


def _to_api_builtins(value: object, form: _AssetForm | None, testnet: Callable[[], bool] | None) -> object:
    """Builtins of the value with assets in the given form - legacy ones in symbols they remember without `testnet`."""
    if isinstance(value, Asset):
        if form is _AssetForm.NAI or (form is None and not value.is_legacy):
            return value.as_nai()
        return value.as_legacy(testnet=None if testnet is None else testnet())
    if dataclasses.is_dataclass(value) and not isinstance(value, type):
        return {
            key: _to_api_builtins(item, field_plan.asset_form if field_plan is not None else None, testnet)
            for key, field_plan, item in _model_items(value)
        }
    if isinstance(value, list | tuple):
        return [_to_api_builtins(item, form, testnet) for item in value]
    if isinstance(value, dict):
        return {key: _to_api_builtins(item, form, testnet) for key, item in value.items()}
    return to_builtins(value)
