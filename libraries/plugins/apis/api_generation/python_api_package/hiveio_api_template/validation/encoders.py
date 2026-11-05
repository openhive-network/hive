"""
Encoding of Hive types to JSON by msgspec (`enc_hook`).

Hive types have a fixed JSON form - they encode themselves: types of public models by `to_builtins` (`Asset`,
`HiveDateTime`), field types unknown to msgspec by `Serializable.serialize` (`JsonString`), and URLs of
hiveio-api-client (`HttpUrl`, `WsUrl`, `P2PUrl` - not `Serializable` since they moved out of hiveio_api) by
`Url.serialize`.
"""

from __future__ import annotations

from pathlib import Path
from typing import Any

from hiveio_api.fields.serializable import Serializable
from hiveio_api_client import Url
from msgspec.json import Encoder


def enc_hook_hf26(obj: Any) -> Any:
    """Encode a value of a type unknown to msgspec (`to_builtins`, `Serializable`, `Url`, `Path`) - `enc_hook`."""
    to_builtins = getattr(obj, "to_builtins", None)
    if callable(to_builtins):
        return to_builtins()
    if isinstance(obj, (Serializable, Url)):
        return obj.serialize()
    if isinstance(obj, Path):
        return str(obj)
    raise NotImplementedError(f"Objects of type {type(obj)} are not supported")


def get_hf26_encoder() -> Encoder:
    return Encoder(enc_hook=enc_hook_hf26)
