from __future__ import annotations

import asyncio
import time
from datetime import datetime, timedelta
from typing import TYPE_CHECKING, Final

from helpy import ContextAsync, ContextSync
from helpy.exceptions import RequestError

if TYPE_CHECKING:
    from types import TracebackType


class DelayGuardBase:
    def __init__(self) -> None:
        self._seconds_to_wait: Final[timedelta] = timedelta(seconds=0.6)
        self._wait_time: Final[float] = 0.1
        self._next_time_unlock: datetime | None = None
        self._exception_occured = False

    def _waiting_should_continue(self) -> bool:
        return self._next_time_unlock is not None and datetime.now() < self._next_time_unlock

    def _handle_exception_impl(self, ex: BaseException, _: TracebackType | None) -> bool:
        self._exception_occured = isinstance(ex, RequestError)
        self._next_time_unlock = datetime.now() + self._seconds_to_wait
        return True

    def _handle_no_exception_impl(self) -> None:
        self._exception_occured = False
        self._next_time_unlock = None

    def error_occured(self) -> bool:
        return self._exception_occured


class SyncDelayGuard(DelayGuardBase, ContextSync["SyncDelayGuard"]):
    def _enter(self) -> SyncDelayGuard:
        while self._waiting_should_continue():
            time.sleep(self._wait_time)
        return self

    def _handle_exception(self, ex: BaseException, tb: TracebackType | None) -> bool:
        return self._handle_exception_impl(ex, tb)

    def _handle_no_exception(self) -> None:
        return self._handle_no_exception_impl()

    def _finally(self) -> None:
        return None


class AsyncDelayGuard(DelayGuardBase, ContextAsync["AsyncDelayGuard"]):
    async def _aenter(self) -> AsyncDelayGuard:
        while self._waiting_should_continue():
            await asyncio.sleep(self._wait_time)
        return self

    async def _ahandle_exception(self, ex: BaseException, tb: TracebackType | None) -> bool:
        return self._handle_exception_impl(ex, tb)

    async def _ahandle_no_exception(self) -> None:
        return self._handle_no_exception_impl()

    async def _afinally(self) -> None:
        return None
