"""
JSON-RPC envelope of API calls.

Responses are built WITHOUT validation - results are builtins and public models (see `hiveio_api._base.HiveModel`).
Validation is opt-in, see `hiveio_api.validate_schema`.
"""

from __future__ import annotations

import json
from dataclasses import dataclass, field
from typing import TYPE_CHECKING, Generic, TypeVar

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


def _jsonrpc_id(value: object) -> JsonRpcIdT:
    """The id of a JSON-RPC call or response - `None` when it is not a number nor a string."""
    return value if isinstance(value, int | str) else None


@dataclass(frozen=True, kw_only=True)
class JSONRPCRequest:
    """A JSON-RPC call - the method (e.g. `database_api.find_accounts`) with its parameters."""

    method: str
    params: object = field(default_factory=dict)
    id_: JsonRpcIdT = 0
    jsonrpc: str = "2.0"

    def json(self) -> str:
        """The call as a JSON-RPC request body - the parameters have to be JSON builtins."""
        return json.dumps({"id": self.id_, "jsonrpc": self.jsonrpc, "method": self.method, "params": self.params})


@dataclass(frozen=True, slots=True)
class JSONRPCResult(Generic[ExpectResultT]):
    """A JSON-RPC response with the result of the call (built without validation, see `build_response`)."""

    result: ExpectResultT
    id_: JsonRpcIdT = 0
    jsonrpc: str = "2.0"


@dataclass(frozen=True, slots=True)
class JSONRPCError:
    """A JSON-RPC response with an error instead of the result - the `error` object as it was received."""

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
        The response with the result built as `expected_type`, or `JSONRPCError` when there is no result. Members of
        the envelope of other types than in JSON-RPC are replaced: `id` by `None`, `jsonrpc` by its text and `error`
        (not an object) by `{"message": <the error>}`.

    Raises:
        hiveio_api.UnsupportedTypeError: When `expected_type` contains other types (e.g. msgspec models or Hive types
            from `schemas.fields`) - such types can be used only for validation.
    """
    build_result = value_builder(expected_type)
    id_ = _jsonrpc_id(response.get("id"))
    jsonrpc = str(response.get("jsonrpc", "2.0"))
    if "result" not in response:
        error = response.get("error", {})
        return JSONRPCError(error=error if isinstance(error, dict) else {"message": error}, id_=id_, jsonrpc=jsonrpc)
    return JSONRPCResult(result=build_result(response["result"]), id_=id_, jsonrpc=jsonrpc)
