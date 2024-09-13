from __future__ import annotations

from typing import TYPE_CHECKING, Any

from beekeepy._handle.beekeeper import Beekeeper as SyncLocalBeekeeper
from beekeepy._handle.beekeeper import close_if_possible
from beekeepy._interface.abc.packed_object import Packed, _RemoteFactoryCallable
from beekeepy._interface.abc.synchronous.beekeeper import Beekeeper as BeekeeperInterface
from beekeepy._interface.delay_guard import SyncDelayGuard
from beekeepy._interface.state_invalidator import StateInvalidator
from beekeepy._interface.synchronous.session import Session
from beekeepy.exceptions.common import DetachRemoteBeekeeperError, UnknownDecisionPathError

if TYPE_CHECKING:
    from beekeepy._handle.beekeeper import SyncRemoteBeekeeper
    from beekeepy._interface.abc.synchronous.session import (
        Session as SessionInterface,
    )


PackedBeekeeper = Packed[BeekeeperInterface]

class Beekeeper(BeekeeperInterface, StateInvalidator):
    def __init__(self, *args: Any, handle: SyncRemoteBeekeeper, unpack_factory: _RemoteFactoryCallable[BeekeeperInterface], **kwargs: Any) -> None:
        super().__init__(*args, **kwargs)
        self.__instance = handle
        self.__guard = SyncDelayGuard()
        self.__unpack_factory = unpack_factory

    def create_session(self, *, salt: str | None = None) -> SessionInterface:  # noqa: ARG002
        session: SessionInterface | None = None
        while session is None or self.__guard.error_occured():
            with self.__guard:
                session = self.__create_session()
                session.get_info()
                return session
        raise UnknownDecisionPathError

    @property
    def session(self) -> SessionInterface:
        return self.__create_session(self._get_instance().session.token)

    def _get_instance(self) -> SyncRemoteBeekeeper:
        return self.__instance

    def teardown(self) -> None:
        close_if_possible(self.__instance)
        self.invalidate()

    def detach(self) -> None:
        if isinstance(self.__instance, SyncLocalBeekeeper):
            self.__instance.detach()
        raise DetachRemoteBeekeeperError

    def __create_session(self, token: str | None = None) -> SessionInterface:
        session = Session(beekeeper=self._get_instance(), use_session_token=token, guard=self.__guard)
        self.register_invalidable(session)
        return session

    def pack(self) -> Packed[BeekeeperInterface]:
        return Packed(settings=self._get_instance().settings, unpack_factory=self.__unpack_factory)
