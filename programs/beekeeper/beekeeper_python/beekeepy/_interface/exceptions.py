from __future__ import annotations

from abc import abstractmethod
from typing import TYPE_CHECKING

from helpy._interfaces.context import ContextSync
from helpy.exceptions import RequestError

if TYPE_CHECKING:
    from types import TracebackType


class DetectableError(ContextSync[None], Exception):
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


class NoWalletWithSuchNameError(DetectableError):
    def __init__(self, wallet_name: str) -> None:
        self.wallet_name = wallet_name
        super().__init__(f"no such wallet with name: {wallet_name}")

    def _is_exception_handled(self, ex: BaseException) -> bool:
        return (
            isinstance(ex, RequestError)
            and "Assert Exception:wallet->load_wallet_file(): Unable to open file: " in ex.error
            and f"{self.wallet_name}.wallet" in ex.error
        )
