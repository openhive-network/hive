from __future__ import annotations

from typing import TYPE_CHECKING, Any

from beekeepy._handle.beekeeper import Beekeeper as SynchronousBeekeeperHandle
from beekeepy._interface.abc.synchronous.beekeeper import Beekeeper as BeekeeperInterface
from beekeepy._interface.synchronous.session import Session

if TYPE_CHECKING:
    from types import TracebackType

    from typing_extensions import Self

    import helpy
    from beekeepy._interface.abc.synchronous.session import (
        Session as SessionInterface,
    )


class Beekeeper(BeekeeperInterface):
    def __init__(self, *args: Any, handle: helpy.Beekeeper, **kwargs: Any) -> None:
        super().__init__(*args, **kwargs)
        self.__instance = handle

    def create_session(self, *, salt: str | None = None) -> SessionInterface:  # noqa: ARG002
        return Session(beekeeper=self._get_instance())

    def _get_instance(self) -> helpy.Beekeeper:
        return self.__instance

    def __exit__(self, _: type[BaseException] | None, ex: BaseException | None, __: TracebackType | None) -> TYPE_CHECKING:
        if isinstance(self.__instance, SynchronousBeekeeperHandle):
            self.__instance.close()
        return super().__exit__(_, ex, __)
