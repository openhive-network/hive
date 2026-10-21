from __future__ import annotations

import json
from dataclasses import dataclass, field
from datetime import UTC, datetime
from functools import cached_property
from typing import TYPE_CHECKING, Literal, Protocol, Self, cast, runtime_checkable

from hiveio_api._communication.jsonrpc import JSONRPCRequest, _jsonrpc_id

if TYPE_CHECKING:
    from collections.abc import Awaitable
    from datetime import timedelta

    from hiveio_api._communication.url import HttpUrl


Methods = Literal["GET", "POST", "PUT"]

JsonValue = dict[str, "JsonValue"] | list["JsonValue"] | str | int | float | bool | None
"""Parsed JSON."""


@dataclass(frozen=True)
class Request:
    """
    Represents a request to be sent.

    Immutable - callbacks preparing the request return a changed copy (`dataclasses.replace`).
    """

    url: HttpUrl
    method: Methods
    headers: dict[str, str]
    body: str | None = None
    timeout: timedelta | None = None

    def get_timeout(self) -> float | None:
        """Returns the timeout in seconds."""
        if self.timeout is None:
            return None
        return self.timeout.total_seconds()

    @cached_property
    def json(self) -> JsonValue:
        """Body parsed once, `None` for a request without body; raises `json.JSONDecodeError` for other bodies."""
        return None if self.body is None else cast("JsonValue", json.loads(self.body))

    @cached_property
    def jsonrpc(self) -> tuple[JSONRPCRequest, ...]:
        """JSON-RPC calls of the request - one call, or all calls of a batch; empty when the body is not JSON-RPC."""
        try:
            payload = self.json
        except json.JSONDecodeError:
            return ()
        calls = payload if isinstance(payload, list) else [payload]
        requests: list[JSONRPCRequest] = []
        for call in calls:
            if not isinstance(call, dict) or not isinstance(method := call.get("method"), str):
                return ()
            requests.append(
                JSONRPCRequest(
                    method=method,
                    params=call.get("params", {}),
                    id_=_jsonrpc_id(call.get("id")),
                    jsonrpc=str(call.get("jsonrpc", "2.0")),
                )
            )
        return tuple(requests)


@dataclass(frozen=True)
class Response:
    """Represents a response received from a request."""

    status_code: int
    headers: dict[str, str]
    body: str

    @cached_property
    def json(self) -> JsonValue:
        """Body parsed once; raises `json.JSONDecodeError` when the body is not JSON."""
        return cast("JsonValue", json.loads(self.body))  # json.loads returns parsed JSON typed as Any


@dataclass
class RequestTimings:
    """Timings of a request - of all its attempts."""

    started: datetime = field(default_factory=lambda: datetime.now(UTC))
    """Start of the first attempt."""

    attempts: list[float] = field(default_factory=list)
    """
    Durations of attempts in seconds - of the first one in `attempts[0]`, of the second one in `attempts[1]`, ...

    Their sum is the duration of the whole request, their count - the number of attempts.
    """


@dataclass(frozen=True)
class RequestContext:
    """Request with its response (or exception) - given to callbacks of the response and errors."""

    request: Request
    response: Response | None = None
    """`None` when the request was not sent or sending it failed."""
    exception: Exception | None = None
    """Exception given to callbacks of errors."""
    timings: RequestTimings = field(default_factory=RequestTimings)

    @property
    def attempt(self) -> int:
        """Number of the attempt (1 for the first one)."""
        return len(self.timings.attempts)


@runtime_checkable
class RequestCallback(Protocol):
    """Callback of `Callbacks.prepare_request` - returns the request to send (a changed copy or the given one)."""

    def __call__(self, request: Request, /) -> Request: ...


@runtime_checkable
class ResponseCallback(Protocol):
    """Callback of `Callbacks.process_response` - called with the context of the received response."""

    def __call__(self, context: RequestContext, /) -> None: ...


@runtime_checkable
class ErrorCallback(Protocol):
    """Callback of `Callbacks.error` - called with the context of the failed request and its exception."""

    def __call__(self, context: RequestContext, /) -> None: ...


@runtime_checkable
class AsyncRequestCallback(Protocol):
    """Asynchronous `RequestCallback` - of `AsyncCallbacks.prepare_request`."""

    def __call__(self, request: Request, /) -> Awaitable[Request]: ...


@runtime_checkable
class AsyncResponseCallback(Protocol):
    """Asynchronous `ResponseCallback` - of `AsyncCallbacks.process_response`."""

    def __call__(self, context: RequestContext, /) -> Awaitable[None]: ...


@runtime_checkable
class AsyncErrorCallback(Protocol):
    """Asynchronous `ErrorCallback` - of `AsyncCallbacks.error`."""

    def __call__(self, context: RequestContext, /) -> Awaitable[None]: ...


SyncCallback = RequestCallback | ResponseCallback | ErrorCallback
AsyncCallback = AsyncRequestCallback | AsyncResponseCallback | AsyncErrorCallback | SyncCallback


@dataclass
class _Stages[RequestCallbackT, ResponseCallbackT, ErrorCallbackT]:
    """Callbacks of the stages of a request (see `Callbacks`)."""

    prepare_request: list[RequestCallbackT] = field(default_factory=list)
    process_response: list[ResponseCallbackT] = field(default_factory=list)
    error: list[ErrorCallbackT] = field(default_factory=list)

    @classmethod
    def joined(cls, *parts: Self) -> Self:
        """Callbacks of all parts - callbacks of each stage in the order of the parts."""
        return cls(
            prepare_request=[callback for part in parts for callback in part.prepare_request],
            process_response=[callback for part in parts for callback in part.process_response],
            error=[callback for part in parts for callback in part.error],
        )


@dataclass
class Callbacks(_Stages[RequestCallback, ResponseCallback, ErrorCallback]):
    """
    Callbacks of a single request - the communicator calls the callbacks of each stage in the order of the lists.

    - `prepare_request` - each one gets the request returned by the previous one and returns the request to send,
    - `process_response` - called with the context of the received response,
    - `error` - called with the context and the exception, when a callback of `prepare_request`, sending the request
      or a callback of `process_response` raised (`response` of the context is `None` unless the response was
      received); the exception is raised further afterwards. A callback may raise another exception instead - it
      replaces the original one and the next `error` callbacks are not called.
    """


@dataclass
class AsyncCallbacks(
    _Stages[
        AsyncRequestCallback | RequestCallback,
        AsyncResponseCallback | ResponseCallback,
        AsyncErrorCallback | ErrorCallback,
    ]
):
    """Callbacks of a single asynchronous request - as `Callbacks`, both synchronous and asynchronous callbacks."""
