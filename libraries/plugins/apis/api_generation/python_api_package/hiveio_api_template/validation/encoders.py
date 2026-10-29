"""
Encoding of Hive types to JSON by msgspec (`enc_hook`).

Hive types have a fixed JSON form - they encode themselves: types of public models by `to_builtins` (`Asset`,
`HiveDateTime`), field types unknown to msgspec by `Serializable.serialize` (`JsonString`).
"""

from __future__ import annotations

from pathlib import Path
from typing import Any

from msgspec.json import Encoder

from hiveio_api.fields.serializable import Serializable


def enc_hook_hf26(obj: Any) -> Any:
    """Encode a value of a type unknown to msgspec (`to_builtins`, `Serializable`, `Path`) - `enc_hook` of msgspec."""
    to_builtins = getattr(obj, "to_builtins", None)
    if callable(to_builtins):
        return to_builtins()
    if isinstance(obj, Serializable):
        return obj.serialize()
    if isinstance(obj, Path):
        return str(obj)
    raise NotImplementedError(f"Objects of type {type(obj)} are not supported")


def get_hf26_encoder() -> Encoder:
    return Encoder(enc_hook=enc_hook_hf26)
