from __future__ import annotations

import asyncio
import contextlib
import json
import time
from contextlib import contextmanager
from datetime import datetime
from json import JSONDecodeError
from typing import TYPE_CHECKING, Any, Final

import requests as rq
import aiohttp

from .clive_exceptions import CliveError, CommunicationError

if TYPE_CHECKING:
    from collections.abc import Iterator



class CustomJSONEncoder(json.JSONEncoder):
    TIME_FORMAT_WITH_MILLIS: Final[str] = "%Y-%m-%dT%H:%M:%S.%f"

    def default(self, obj: Any) -> Any:
        if isinstance(obj, datetime):
            return obj.strftime(self.TIME_FORMAT_WITH_MILLIS)

        return super().default(obj)


class ErrorInResponseJsonError(CliveError):
    """Raised if "error" field found in response json."""


class Communication:
    DEFAULT_ATTEMPTS: Final[int] = 5
    DEFAULT_TIMEOUT_TOTAL_SECONDS: Final[float] = 3
    DEFAULT_POOL_TIME_SECONDS: Final[float] = 0.2

    def __init__(
        self,
        *,
        max_attempts: int = DEFAULT_ATTEMPTS,
        timeout_secs: float = DEFAULT_TIMEOUT_TOTAL_SECONDS,
        pool_time_secs: float = DEFAULT_POOL_TIME_SECONDS,
    ):
        self.__max_attempts = max_attempts
        self.__timeout_secs = timeout_secs
        self.__pool_time_secs = pool_time_secs

        assert self.__max_attempts > 0, "Max attempts must be greater than 0."
        assert self.__timeout_secs > 0, "Timeout must be greater than 0."
        assert self.__pool_time_secs >= 0, "Pool time must be greater or equal to 0."

    @contextmanager
    def modified_connection_details(
        self,
        max_attempts: int = DEFAULT_ATTEMPTS,
        timeout_secs: float = DEFAULT_TIMEOUT_TOTAL_SECONDS,
        pool_time_secs: float = DEFAULT_POOL_TIME_SECONDS,
    ) -> Iterator[None]:
        """Allows to temporarily change connection details."""
        before = {
            "max_attempts": self.__max_attempts,
            "timeout_secs": self.__timeout_secs,
            "pool_time_secs": self.__pool_time_secs,
        }
        self.__max_attempts = max_attempts
        self.__timeout_secs = timeout_secs
        self.__pool_time_secs = pool_time_secs
        try:
            yield
        finally:
            self.__max_attempts = before["max_attempts"]  # type: ignore[assignment]
            self.__timeout_secs = before["timeout_secs"]
            self.__pool_time_secs = before["pool_time_secs"]

    def request(  # noqa: PLR0913
        self,
        url: str,
        *,
        data: Any,
        max_attempts: int | None = None,
        timeout_secs: float | None = None,
        pool_time_secs: float | None = None,
    ) -> aiohttp.ClientResponse:
        """
        Make a single sync request to the given url.

        Args:
        ----
        =====
        url: url to send request to.
        data: data to send.
        max_attempts: max attempts to send request (will override the value given during Communication creation)
        timeout_secs: timeout in seconds (will override the value given during Communication creation)
        pool_time_secs: time to wait between attempts (will override the value given during Communication creation)
        """
        return self.__request(
                url, data=data, max_attempts=max_attempts, timeout_secs=timeout_secs, pool_time_secs=pool_time_secs
            )

    async def arequest(  # noqa: PLR0913
        self,
        url: str,
        *,
        data: Any,
        max_attempts: int | None = None,
        timeout_secs: float | None = None,
        pool_time_secs: float | None = None,
    ) -> aiohttp.ClientResponse:
        """
        Make a single async request to the given url.

        Args:
        ----
        =====
        url: url to send request to.
        data: data to send.
        max_attempts: max attempts to send request (will override the value given during Communication creation)
        timeout_secs: timeout in seconds (will override the value given during Communication creation)
        pool_time_secs: time to wait between attempts (will override the value given during Communication creation)
        """
        return  self.__request(
            url, data=data, max_attempts=max_attempts, timeout_secs=timeout_secs, pool_time_secs=pool_time_secs
        )

    def __request(  # noqa: PLR0913, C901
        self,
        url: str,
        *,
        data: Any,
        max_attempts: int | None = None,
        timeout_secs: float | None = None,
        pool_time_secs: float | None = None,
    ) -> aiohttp.ClientResponse:
        _max_attempts = max_attempts or self.__max_attempts
        _timeout_secs = timeout_secs or self.__timeout_secs
        _pool_time_secs = pool_time_secs or self.__pool_time_secs

        assert _max_attempts > 0, "Max attempts must be greater than 0."
        assert _timeout_secs > 0, "Timeout must be greater than 0."
        assert _pool_time_secs >= 0, "Pool time must be greater or equal to 0."

        result: CommunicationResponseT | None = None
        exception: Exception | None = None

        data_serialized = data if isinstance(data, str) else json.dumps(data, cls=CustomJSONEncoder)

        attempt = 0
        timeouts_count = 0

        while attempt < _max_attempts:
            response = rq.post(
                url,
                data=data_serialized,
                headers={"Content-Type": "application/json"},
            )
            if response.ok:
                return response.json()

        raise CommunicationError(url, data_serialized, result) from exception

    @classmethod
    def __check_response(cls, url: str, request: str, result: Any) -> None:
        if isinstance(result, dict):
            cls.__check_response_item(url=url, request=request, response=result, item=result)
        elif isinstance(result, list):
            for item in result:
                cls.__check_response_item(url=url, request=request, response=result, item=item)
        else:
            raise Exception(url, request, result)

    @classmethod
    def __check_response_item(cls, url: str, request: str, response: Any, item: JsonT) -> None:
        if "error" in item:
            raise ErrorInResponseJsonError
        if "result" not in item:
            raise Exception(url, request, response)
