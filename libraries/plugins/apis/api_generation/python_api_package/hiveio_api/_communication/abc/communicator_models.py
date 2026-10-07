from __future__ import annotations

from dataclasses import dataclass
from typing import TYPE_CHECKING, Literal, Protocol, runtime_checkable

if TYPE_CHECKING:
    from collections.abc import Awaitable
    from datetime import timedelta

    from hiveio_api._communication.url import HttpUrl


Methods = Literal["GET", "POST", "PUT"]


@dataclass
class Request:
    """Represents a request to be sent."""

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


@dataclass
class Response:
    """Represents a response received from a request."""

    status_code: int
    headers: dict[str, str]
    body: str


@runtime_checkable
class RequestCallback(Protocol):
    def __call__(self, *, request: Request) -> Request: ...


@runtime_checkable
class ResponseCallback(Protocol):
    def __call__(self, *, request: Request, response: Response) -> None: ...


@runtime_checkable
class ErrorCallback(Protocol):
    def __call__(self, *, request: Request, response: Response | None, exception: Exception) -> None: ...


@runtime_checkable
class AsyncRequestCallback(Protocol):
    def __call__(self, *, request: Request) -> Awaitable[Request]: ...


@runtime_checkable
class AsyncResponseCallback(Protocol):
    def __call__(self, *, request: Request, response: Response) -> Awaitable[None]: ...


@runtime_checkable
class AsyncErrorCallback(Protocol):
    def __call__(self, *, request: Request, response: Response | None, exception: Exception) -> Awaitable[None]: ...


SyncCallback = RequestCallback | ResponseCallback | ErrorCallback
AsyncCallback = AsyncRequestCallback | AsyncResponseCallback | AsyncErrorCallback | SyncCallback


@dataclass
class Callbacks:
    prepare_request: RequestCallback | None = None
    process_response: ResponseCallback | None = None
    request_error: ErrorCallback | None = None
    response_error: ErrorCallback | None = None
    communicator_error: ErrorCallback | None = None


@dataclass
class AsyncCallbacks:
    prepare_request: AsyncRequestCallback | RequestCallback | None = None
    process_response: AsyncResponseCallback | ResponseCallback | None = None
    request_error: AsyncErrorCallback | ErrorCallback | None = None
    response_error: AsyncErrorCallback | ErrorCallback | None = None
    communicator_error: AsyncErrorCallback | ErrorCallback | None = None
