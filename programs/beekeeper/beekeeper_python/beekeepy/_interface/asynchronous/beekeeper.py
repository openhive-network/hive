from __future__ import annotations

from typing import TYPE_CHECKING, Any

from beekeepy._handle.beekeeper import AsyncBeekeeper as AsyncLocalBeekeeper
from beekeepy._handle.beekeeper import close_if_possible
from beekeepy._interface.abc.asynchronous.beekeeper import Beekeeper as BeekeeperInterface
from beekeepy._interface.abc.packed_object import Packed, _RemoteFactoryCallable
from beekeepy._interface.asynchronous.session import Session
from beekeepy._interface.delay_guard import AsyncDelayGuard
from beekeepy._interface.state_invalidator import StateInvalidator
from beekeepy.exceptions import UnknownDecisionPathError
from beekeepy.exceptions.common import DetachRemoteBeekeeperError

if TYPE_CHECKING:
    from beekeepy._handle.beekeeper import AsyncRemoteBeekeeper
    from beekeepy._interface.abc.asynchronous.session import (
        Session as SessionInterface,
    )

PackedAsyncBeekeeper = Packed[BeekeeperInterface]

class Beekeeper(BeekeeperInterface, StateInvalidator):
    def __init__(self, *args: Any, handle: AsyncRemoteBeekeeper, unpack_factory: _RemoteFactoryCallable[BeekeeperInterface], **kwargs: Any) -> None:
        super().__init__(*args, **kwargs)
        self.__instance = handle
        self.__guard = AsyncDelayGuard()
        self.__unpack_factory = unpack_factory

    async def create_session(self, *, salt: str | None = None) -> SessionInterface:  # noqa: ARG002
        session: SessionInterface | None = None
        while session is None or self.__guard.error_occured():
            async with self.__guard:
                session = self.__create_session()
                await session.get_info()
                return session
        raise UnknownDecisionPathError

    @property
    async def session(self) -> SessionInterface:
        return self.__create_session((await self._get_instance().session).token)

    def _get_instance(self) -> AsyncRemoteBeekeeper:
        return self.__instance

    def teardown(self) -> None:
        close_if_possible(self.__instance)
        self.invalidate()

    def detach(self) -> None:
        if isinstance(self.__instance, AsyncLocalBeekeeper):
            self.__instance.detach()
        raise DetachRemoteBeekeeperError

    def __create_session(self, token: str | None = None) -> SessionInterface:
        session = Session(beekeeper=self._get_instance(), use_session_token=token, guard=self.__guard)
        self.register_invalidable(session)
        return session

    def pack(self) -> PackedAsyncBeekeeper:
        return Packed(settings=self._get_instance().settings, unpack_factory=self.__unpack_factory)
