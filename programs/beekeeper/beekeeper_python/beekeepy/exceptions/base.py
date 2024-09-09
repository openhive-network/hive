from __future__ import annotations

from abc import ABC, abstractmethod
from typing import TYPE_CHECKING

from helpy import ContextSync
from helpy.exceptions import HelpyError

if TYPE_CHECKING:
    from types import TracebackType


class BeekeepyError(HelpyError, ABC):
    """Base class for all exception raised by beekeepy."""


class BeekeeperExecutableError(BeekeepyError, ABC):
    """Base class for exceptions related to starting/closing beekeeper executable."""


class DetectableError(ContextSync[None], BeekeepyError, ABC):
    """
    Base class for conditionally raised exception using `with` statement based detection.

    Example:
    ```
    with DividsionByZeroException(a, b):
        print(a / b)
    ```

    Raises:
        self: If conditions specified by child classes in `_is_exception_handled` are met
    """

    @abstractmethod
    def _is_exception_handled(self, ex: BaseException) -> bool: ...

    def _enter(self) -> None:
        return None

    def _finally(self) -> None:
        return None

    def _handle_exception(self, ex: BaseException, tb: TracebackType | None) -> bool:
        if self._is_exception_handled(ex):
            raise self
        return super()._handle_exception(ex, tb)
