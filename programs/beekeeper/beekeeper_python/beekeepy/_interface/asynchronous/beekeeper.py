from __future__ import annotations

from typing import TYPE_CHECKING, Any

from loguru import logger

from beekeepy._handle.beekeeper import AsyncBeekeeper as AsynchronousBeekeeperHandle
from beekeepy._handle.beekeeper import AsyncRemoteBeekeeper as AsynchronousRemoteBeekeeperHandle
from beekeepy._handle.beekeeper import close_if_possible
from beekeepy._interface.abc.asynchronous.beekeeper import Beekeeper as BeekeeperInterface
from beekeepy._interface.abc.packed_object import PackedAsyncBeekeeper
from beekeepy._interface.asynchronous.session import Session
from beekeepy._interface.delay_guard import AsyncDelayGuard
from beekeepy._interface.settings import Settings
from beekeepy._interface.state_invalidator import StateInvalidator
from beekeepy.exceptions import BeekeeperAlreadyRunningError, DetachRemoteBeekeeperError, UnknownDecisionPathError

if TYPE_CHECKING:
    from helpy import HttpUrl

    from beekeepy._handle.beekeeper import AsyncRemoteBeekeeper
    from beekeepy._interface.abc.asynchronous.session import (
        Session as SessionInterface,
    )


class Beekeeper(BeekeeperInterface, StateInvalidator):
    def __init__(self, *args: Any, handle: AsyncRemoteBeekeeper, **kwargs: Any) -> None:
        super().__init__(*args, **kwargs)
        self.__instance = handle
        self.__guard = AsyncDelayGuard()

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
        if not isinstance(self.__instance, AsynchronousBeekeeperHandle):
            raise DetachRemoteBeekeeperError
        self.__instance.detach()

    def __create_session(self, token: str | None = None) -> SessionInterface:
        session = Session(beekeeper=self._get_instance(), use_session_token=token, guard=self.__guard)
        self.register_invalidable(session)
        return session

    def pack(self) -> PackedAsyncBeekeeper:
        return PackedAsyncBeekeeper(settings=self._get_instance().settings, unpack_factory=Beekeeper._remote_factory)

    @classmethod
    async def _factory(cls, *, settings: Settings | None = None) -> Beekeeper:
        settings = settings or Settings()
        handle = AsynchronousBeekeeperHandle(settings=settings, logger=logger)
        cls.__apply_existing_session_token(settings=settings, handle=handle)
        try:
            handle.run()
        except BeekeeperAlreadyRunningError as err:
            settings.http_endpoint = err.address
            return await cls._remote_factory(url_or_settings=settings)
        return cls(handle=handle)

    @classmethod
    async def _remote_factory(cls, *, url_or_settings: Settings | HttpUrl) -> Beekeeper:
        settings = url_or_settings if isinstance(url_or_settings, Settings) else Settings(http_endpoint=url_or_settings)
        handle = AsynchronousRemoteBeekeeperHandle(settings=settings)
        cls.__apply_existing_session_token(settings=settings, handle=handle)
        return cls(handle=handle)

    @classmethod
    def __apply_existing_session_token(cls, settings: Settings, handle: AsynchronousRemoteBeekeeperHandle) -> None:
        if settings.use_existing_session:
            handle.set_session_token(settings.use_existing_session)
