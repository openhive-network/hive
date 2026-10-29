from __future__ import annotations

from typing import Annotated

import msgspec

from hiveio_api.fields.hive_constants import (
    HIVE_TIME_PATTERN,
    MAX_INT64_VALUE,
    MIN_INT64_VALUE,
    MIN_UINT64_VALUE,
)

__all__ = [
    "HiveDateTime",
    "HiveInt",
    "Int64t",
    "JsonString",
    "Uint64t",
]

HiveDateTime = Annotated[str, msgspec.Meta(pattern=HIVE_TIME_PATTERN)]
"""Time in the format of Hive - checked by the pattern, not parsed."""

# integers - also strings of digits, as hived sends integers which do not fit doubles of JSON
HiveInt = int | Annotated[str, msgspec.Meta(pattern=r"^-?\d+$")]
Int64t = (
    Annotated[int, msgspec.Meta(ge=MIN_INT64_VALUE, le=MAX_INT64_VALUE)]
    | Annotated[str, msgspec.Meta(pattern=r"^-?\d{1,19}$")]
)
# msgspec takes no bounds beyond int64 - the maximum of Uint64t is not checked
Uint64t = Annotated[int, msgspec.Meta(ge=MIN_UINT64_VALUE)] | Annotated[str, msgspec.Meta(pattern=r"^\d{1,20}$")]

JsonString = str
"""JSON embedded in a string (`json` of `custom_json`) - its content is not checked (no pattern can check JSON)."""
