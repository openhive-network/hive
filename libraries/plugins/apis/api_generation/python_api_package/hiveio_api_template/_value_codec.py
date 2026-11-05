"""
Values of API calls with Hive types - the `ValueCodec` hiveio_api installs in hiveio-api-client (`import hiveio_api`).

Parameters are converted by `hiveio_api._base.to_api_builtins` (public models with keys of the API, assets in the form
of their fields, legacy ones in symbols of the network), validating Hive types (`hiveio_api.fields`, msgspec models) in
the form given by their type, results are built by `hiveio_api._base.value_builder` (public models, without validation).

The modules doing it are imported when a value is converted - installing the codec loads neither models nor the
validation machinery.
"""

from __future__ import annotations

from typing import TYPE_CHECKING, Final, TypeVar

from hiveio_api_client import ValueCodec

if TYPE_CHECKING:
    from collections.abc import Callable

T = TypeVar("T")


def _to_api_builtins(value: object, testnet: Callable[[], bool]) -> object:
    from hiveio_api._base import to_api_builtins

    return to_api_builtins(value, testnet=testnet)


def _value_builder(annotation: type[T]) -> Callable[[object], T]:
    from hiveio_api._base import value_builder

    builder: Callable[[object], T] = value_builder(annotation)
    return builder


def _validating_type_to_builtins(value: object) -> object:
    """Builtins of values `json` can not serialize - validating Hive types of `hiveio_api.fields` (msgspec models)."""
    import msgspec

    from hiveio_api.validation.encoders import enc_hook_hf26

    return msgspec.to_builtins(value, enc_hook=enc_hook_hf26)


HIVE_VALUE_CODEC: Final[ValueCodec] = ValueCodec(
    to_api_builtins=_to_api_builtins,
    value_builder=_value_builder,
    json_default=_validating_type_to_builtins,
)
