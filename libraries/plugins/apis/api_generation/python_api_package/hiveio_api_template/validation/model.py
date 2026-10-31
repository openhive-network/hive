"""
Common configuration for all classes(except some hive_fields), here you can add config that all classes which
inheritance from this class must have
"""

from __future__ import annotations

import copy
from json import dumps as pretty_json_dumps
from pathlib import Path
from typing import TYPE_CHECKING, Any, Literal, Self, cast

import msgspec

if TYPE_CHECKING:
    from collections.abc import Iterable

    from hiveio_api.validation.decoders import DecoderFactory

DictStrAny = dict[str, Any]


class PreconfiguredBaseModel(msgspec.Struct, omit_defaults=True, forbid_unknown_fields=True):
    """Base of models of Hive data - msgspec structs rejecting unknown fields, with helpers of (de)serialization."""

    @classmethod
    def __is_aliased_field_name(cls, field_name: str) -> bool:
        return field_name in {"id", "from", "json", "schema", "open", "field", "input", "hex", "type"}

    def __getitem__(self, key: str | int) -> Any:
        """
        This allows using any model as dictionary
        """
        key_str = self.__get_field_name(key)
        value = getattr(self, key_str)
        if isinstance(key, int):
            """
            This if statement is support for direct dict conversions:
            ```python
            dict(ChildOdPreconfiguredBaseModel())
            ```
            Alternative solution is to implement __iter__ but it was
            abandoned in favor of this solution, because __iter__
            is used in different projects.
            """
            return [key_str, value]
        return value

    def __setitem__(self, key: str, value: Any) -> None:
        """
        This allows using any model as dictionary
        """
        key = self.__get_field_name(key)
        setattr(self, key, value)

    def __get_field_name(self, name: str | int) -> str:
        if isinstance(name, int):
            return self.__struct_fields__[name]
        if not hasattr(self, name) and self.__is_aliased_field_name(name):
            name = f"{name}_"

        assert hasattr(
            self, name
        ), f"`{name}` does not exists in `{self.__class__.__name__}`, available are: {list(self.dict().keys())}"
        return name

    def __contains__(self, key: str) -> bool:
        return key in self.dict()

    def json(  # noqa: PLR0913
        self,
        *,
        str_keys: bool = False,
        builtin_types: Iterable[type] | None = None,
        order: Literal[None, "deterministic", "sorted"] = None,
        exclude_none: bool = False,
        remove_whitespaces: bool = False,
        exclude: set[str] | None = None,
        indent: int | None = None,
    ) -> str:
        data = self.__as_builtins(
            str_keys=str_keys, builtin_types=builtin_types, order=order, exclude_none=exclude_none, exclude=exclude
        )

        if remove_whitespaces:
            return msgspec.json.encode(data, order=order).decode()
        return pretty_json_dumps(data, sort_keys=(order == "sorted"), ensure_ascii=False, indent=indent)

    def dict(
        self,
        *,
        exclude: set[str] | None = None,
        exclude_none: bool = False,
        exclude_defaults: bool = False,
    ) -> DictStrAny:
        data: DictStrAny = {}
        defaults = (
            dict(zip(self.__struct_fields__, self.__struct_defaults__, strict=False))
            if hasattr(self, "__struct_defaults__")
            else {}
        )

        struct_data = msgspec.structs.asdict(self)

        for key, value in struct_data.items():
            key_clean = key.strip("_")

            if exclude_none and value is None:
                continue

            if exclude_defaults and key_clean in defaults and value == defaults[key_clean]:
                continue

            if exclude is not None and key_clean in exclude:
                continue

            data[key_clean] = value

        return data

    def __as_builtins(
        self,
        str_keys: bool = False,
        builtin_types: Iterable[type] | None = None,
        order: Literal[None, "deterministic", "sorted"] = None,
        exclude_none: bool = False,
        exclude: set[str] | None = None,
    ) -> DictStrAny:
        from hiveio_api.validation.encoders import enc_hook_hf26

        data: DictStrAny = msgspec.to_builtins(
            obj=self._get_object_for_serialization(),
            enc_hook=enc_hook_hf26,
            str_keys=str_keys,
            builtin_types=builtin_types,
            order=order,
        )
        if exclude_none:
            data = {key: value for key, value in data.items() if value is not None}

        if exclude is not None:
            return {k: v for k, v in data.items() if k not in exclude}
        return data

    def _get_object_for_serialization(self) -> Self:
        return self

    @classmethod
    def parse_file(cls, path: Path, custom_decoder_factory: DecoderFactory | None = None) -> Self:
        # Default decoder is hf26_decoder
        if custom_decoder_factory is None:
            from hiveio_api.validation.decoders import get_hf26_decoder

            custom_decoder_factory = get_hf26_decoder
        with Path.open(path, encoding="utf-8") as file:
            raw = file.read()
            return cls.parse_raw(raw, custom_decoder_factory)

    @classmethod
    def parse_raw(cls, raw: str, custom_decoder_factory: DecoderFactory | None = None) -> Self:
        # Default decoder is hf26_decoder
        if custom_decoder_factory is None:
            from hiveio_api.validation.decoders import get_hf26_decoder

            custom_decoder_factory = get_hf26_decoder
        decoder = custom_decoder_factory(cls)
        return cast(Self, decoder.decode(raw))

    @classmethod
    def parse_builtins(cls, raw: Any) -> Self:
        """
        Build the model from JSON builtins (e.g. from `json.loads`), converted by msgspec with `dec_hook_hf26`.

        Raises:
            msgspec.ValidationError: When the data does not match the model.
        """
        from hiveio_api.validation.decoders import dec_hook_hf26, first_use_guarded

        with first_use_guarded(cls):
            return msgspec.convert(obj=raw, type=cls, dec_hook=dec_hook_hf26)

    def copy(self, exclude: set[str] | None = None) -> Self:
        copied = copy.deepcopy(self)
        if exclude is not None:
            for to_exclude in exclude:
                setattr(copied, to_exclude, None)
        return copied

    @classmethod
    def excluded_fields_for_schema_json(cls) -> set[str]:
        """
        By default it returns empty set, but can be overrided by sub-classes to return fields
        that should be excluded during schema serialization.

        eg:
        class ParentModel(PreconfiguredBaseModel):
            a: int
            b: str

            @classmethod
            def excluded_fields_for_schema_json(cls) -> set[str]:
                return {"b"}

        class ChildModel(ParentModel):
            c: int
            d: str

            @classmethod
            def excluded_fields_for_schema_json(cls) -> set[str]:
                return {"d", *super().excluded_fields_for_schema_json()}
        """
        return set()

    @classmethod
    def schema_json(cls) -> str:
        """
        Use function 'excluded_fields_for_schema_json' to exclude fields from schema.
        """
        from hiveio_api.validation._json_schema import schema, schema_hook

        return msgspec.json.encode(schema(cls, schema_hook=schema_hook)).decode()

    def __hash__(self) -> int:
        return hash(self.json())

    def humanize(self) -> str:
        """
        Returns a human-readable string representation of the model.
        """
        from hiveio_api.validation.encoders import get_hf26_encoder

        encoder = get_hf26_encoder()

        return " ".join(f"{k}={encoder.encode(v).decode()}" for k, v in self.dict(exclude_none=True).items())

    def __str__(self) -> str:
        return self.humanize()
