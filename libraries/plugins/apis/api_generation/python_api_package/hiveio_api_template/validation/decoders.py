"""
Decoding of Hive types from JSON by msgspec (`dec_hook`).

Hive types have a fixed JSON form - types of hand-written models unknown to msgspec (`HiveDateTime`, `JsonString`)
decode themselves (`Serializable.deserialize`). Validation models need no hooks - their forms
(`hiveio_api.fields.validation`) are checked by msgspec itself.
"""

from __future__ import annotations

import threading
from collections.abc import Callable, Iterator
from contextlib import contextmanager
from pathlib import Path
from typing import Any, Final, get_origin

import msgspec
from msgspec.json import Decoder

from hiveio_api.fields.serializable import Serializable

DecoderFactory = Callable[[type[msgspec.Struct]], Decoder[msgspec.Struct]]

_prepared_types: set[object] = set()
"""Types already used once - msgspec has built the information about them."""
_preparation_lock: Final = threading.Lock()


def dec_hook_hf26(type_: type, obj: Any) -> Any:
    """Decode a value of a type unknown to msgspec (`Serializable`, `Path`, `object`) - `dec_hook` of msgspec."""
    cls: type = get_origin(type_) or type_  # generic types (e.g. `JsonString[object]`) are given with their parameters
    if cls is object:  # any value (e.g. an item of a free-form JSON array) is kept as it is
        return obj
    if issubclass(cls, Serializable):
        return cls.deserialize(obj)
    if cls is Path:
        return Path(obj)
    raise NotImplementedError(f"Objects of type {type_} are not supported")


def get_hf26_decoder(type_: type[msgspec.Struct]) -> Decoder[msgspec.Struct]:
    """JSON decoder of the model with `dec_hook_hf26` - created within `first_use_guarded`."""
    with first_use_guarded(type_):
        return msgspec.json.Decoder(type_, dec_hook=dec_hook_hf26)


@contextmanager
def first_use_guarded(type_: object) -> Iterator[None]:
    """
    Guard a use of the type by msgspec (decoding, conversion) - the first use is made by one thread at a time.

    msgspec builds the information about a type (resolving its annotations) on its first use, which is not thread safe
    (concurrent first uses may crash the interpreter).
    """
    if type_ in _prepared_types:
        yield
        return
    with _preparation_lock:
        try:
            yield
        finally:
            _prepared_types.add(type_)
