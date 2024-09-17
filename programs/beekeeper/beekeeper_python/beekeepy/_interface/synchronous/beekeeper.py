from __future__ import annotations

from typing import TYPE_CHECKING, Any

from loguru import logger

from beekeepy._handle.beekeeper import Beekeeper as SynchronousBeekeeperHandle
from beekeepy._handle.beekeeper import SyncRemoteBeekeeper as SynchronousRemoteBeekeeperHandle
from beekeepy._handle.beekeeper import close_if_possible
from beekeepy._interface.abc.packed_object import PackedSyncBeekeeper
from beekeepy._interface.abc.synchronous.beekeeper import Beekeeper as BeekeeperInterface
from beekeepy._interface.delay_guard import SyncDelayGuard
from beekeepy._interface.settings import Settings
from beekeepy._interface.state_invalidator import StateInvalidator
from beekeepy._interface.synchronous.session import Session
from beekeepy.exceptions import (
    BeekeeperAlreadyRunningError,
    DetachRemoteBeekeeperError,
    UnknownDecisionPathError,
)

if TYPE_CHECKING:
    from helpy import HttpUrl

    from beekeepy._handle.beekeeper import SyncRemoteBeekeeper
    from beekeepy._interface.abc.synchronous.session import (
        Session as SessionInterface,
    )


class Beekeeper(BeekeeperInterface, StateInvalidator):
    def __init__(self, *args: Any, handle: SyncRemoteBeekeeper, **kwargs: Any) -> None:
        super().__init__(*args, **kwargs)
        self.__instance = handle
        self.__guard = SyncDelayGuard()

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
        if not isinstance(self.__instance, SynchronousBeekeeperHandle):
            raise DetachRemoteBeekeeperError
        self.__instance.detach()

    def __create_session(self, token: str | None = None) -> SessionInterface:
        session = Session(beekeeper=self._get_instance(), use_session_token=token, guard=self.__guard)
        self.register_invalidable(session)
        return session

    def pack(self) -> PackedSyncBeekeeper:
        return PackedSyncBeekeeper(settings=self._get_instance().settings, unpack_factory=Beekeeper._remote_factory)

    @classmethod
    def _factory(cls, *, settings: Settings | None = None) -> Beekeeper:
        settings = settings or Settings()
        handle = SynchronousBeekeeperHandle(settings=settings, logger=logger)
        cls.__apply_existing_session_token(settings=settings, handle=handle)
        try:
            handle.run()
        except BeekeeperAlreadyRunningError as err:
            settings.http_endpoint = err.address
            return cls._remote_factory(url_or_settings=settings)
        return cls(handle=handle)

    @classmethod
    def _remote_factory(cls, *, url_or_settings: Settings | HttpUrl) -> Beekeeper:
        settings = url_or_settings if isinstance(url_or_settings, Settings) else Settings(http_endpoint=url_or_settings)
        handle = SynchronousRemoteBeekeeperHandle(settings=settings)
        cls.__apply_existing_session_token(settings=settings, handle=handle)
        return cls(handle=handle)

    @classmethod
    def __apply_existing_session_token(cls, settings: Settings, handle: SynchronousRemoteBeekeeperHandle) -> None:
        if settings.use_existing_session:
            handle.set_session_token(settings.use_existing_session)
