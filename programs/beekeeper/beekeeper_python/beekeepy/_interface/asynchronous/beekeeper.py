from __future__ import annotations

from typing import TYPE_CHECKING, Any

from beekeepy._interface.abc.asynchronous.beekeeper import Beekeeper as BeekeeperInterface
from beekeepy._interface.asynchronous.session import Session

if TYPE_CHECKING:
    from beekeepy._handle.beekeeper import AsyncBeekeeper as AsynchronousBeekeeperHandle
    from beekeepy._interface.abc.asynchronous.session import (
        Session as SessionInterface,
    )


class Beekeeper(BeekeeperInterface):
    def __init__(self, *args: Any, handle: AsynchronousBeekeeperHandle, **kwargs: Any) -> None:
        super().__init__(*args, **kwargs)
        self.__instance = handle

    async def create_session(self, *, salt: str | None = None) -> SessionInterface:  # noqa: ARG002
        return Session(beekeeper=self._get_instance())

    def _get_instance(self) -> AsynchronousBeekeeperHandle:
        return self.__instance

    def delete(self) -> None:
        self.__instance.close()
