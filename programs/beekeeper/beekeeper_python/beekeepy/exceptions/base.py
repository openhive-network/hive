from __future__ import annotations

from abc import ABC, abstractmethod
from typing import TYPE_CHECKING

from pydantic import StrRegexError

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
    def _is_exception_handled(self, ex: BaseException) -> bool:
        ...

    def _enter(self) -> None:
        return None

    def _finally(self) -> None:
        return None

    def _handle_exception(self, ex: BaseException, tb: TracebackType | None) -> bool:
        if self._is_exception_handled(ex):
            raise self
        return super()._handle_exception(ex, tb)


class SchemaDetectableError(DetectableError, ABC):
    """Base class for errors that bases on schema exceptions."""

    def __init__(self, arg_name: str, arg_value: str) -> None:
        """Constructor.

        Args:
            arg_name (str): name of argument which was invalid
            arg_value (str): value of argument which was invalid
        """
        self._arg_name = arg_name
        self._arg_value = arg_value
        super().__init__(self._error_message())

    def _is_exception_handled(self, ex: BaseException) -> bool:
        return isinstance(ex, StrRegexError)

    @abstractmethod
    def _error_message(self) -> str:
        ...
