from __future__ import annotations
from types import TracebackType

from typing import TYPE_CHECKING, Any

from beekeepy._handle.beekeeper import AsyncBeekeeper as AsynchronousBeekeeperHandle
from beekeepy._interface.abc.asynchronous.beekeeper import Beekeeper as BeekeeperInterface
from beekeepy._interface.asynchronous.session import Session

if TYPE_CHECKING:
    import helpy
    from beekeepy._interface.abc.asynchronous.session import (
        Session as SessionInterface,
    )


class Beekeeper(BeekeeperInterface):
    def __init__(self, *args: Any, handle: helpy.AsyncBeekeeper, **kwargs: Any) -> None:
        super().__init__(*args, **kwargs)
        self.__instance = handle

    async def create_session(self, *, salt: str | None = None) -> SessionInterface:  # noqa: ARG002
        return Session(beekeeper=self._get_instance())

    def _get_instance(self) -> helpy.AsyncBeekeeper:
        return self.__instance


    def __exit__(self, _: type[BaseException] | None, ex: BaseException | None, __: TracebackType | None) -> TYPE_CHECKING:
        if isinstance(self.__instance, AsynchronousBeekeeperHandle):
            self.__instance.close()
        return super().__exit__(_, ex, __)
