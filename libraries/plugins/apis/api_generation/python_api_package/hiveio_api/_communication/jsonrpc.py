"""
JSON-RPC envelope of API calls.

Responses are built WITHOUT validation - results are builtins and public models (see `hiveio_api._base.HiveModel`).
Validation is opt-in, see `hiveio_api.validate_schema`.
"""

from __future__ import annotations

import json
from dataclasses import dataclass, field
from typing import TYPE_CHECKING, Generic, TypeVar, cast

from hiveio_api._base import value_builder

if TYPE_CHECKING:
    from collections.abc import Mapping

__all__ = [
    "ExpectResultT",
    "JSONRPCError",
    "JSONRPCRequest",
    "JSONRPCResult",
    "build_response",
]

ExpectResultT = TypeVar("ExpectResultT")

JsonRpcIdT = int | str | None


@dataclass(frozen=True, kw_only=True)
class JSONRPCRequest:
    method: str
    params: object = field(default_factory=dict)
    id_: JsonRpcIdT = 0
    jsonrpc: str = "2.0"

    def json(self) -> str:
        return json.dumps({"id": self.id_, "jsonrpc": self.jsonrpc, "method": self.method, "params": self.params})


@dataclass(frozen=True, slots=True)
class JSONRPCResult(Generic[ExpectResultT]):
    result: ExpectResultT
    id_: JsonRpcIdT = 0
    jsonrpc: str = "2.0"


@dataclass(frozen=True, slots=True)
class JSONRPCError:
    error: dict[str, object]
    id_: JsonRpcIdT = 0
    jsonrpc: str = "2.0"


def build_response(
    expected_type: type[ExpectResultT], response: Mapping[str, object]
) -> JSONRPCResult[ExpectResultT] | JSONRPCError:
    """
    Build the response of an API call from parsed JSON-RPC response, without validation.

    Args:
        expected_type: Expected type of the result - builtins and public models, also in lists, dicts and unions.
        response: Parsed JSON-RPC response (e.g. result of `json.loads`).

    Returns:
        The response with the result built as `expected_type`, or `JSONRPCError` when there is no result.

    Raises:
        hiveio_api.UnsupportedTypeError: When `expected_type` contains other types (e.g. msgspec models or Hive types
            from `schemas.fields`) - such types can be used only for validation.
    """
    build_result = value_builder(expected_type)
    # members of the envelope are taken as they are - not validated, as the result
    id_ = cast("JsonRpcIdT", response.get("id"))
    jsonrpc = cast("str", response.get("jsonrpc", "2.0"))
    if "result" not in response:
        return JSONRPCError(error=cast("dict[str, object]", response.get("error", {})), id_=id_, jsonrpc=jsonrpc)
    return JSONRPCResult(result=build_result(response["result"]), id_=id_, jsonrpc=jsonrpc)
