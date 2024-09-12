from __future__ import annotations

from typing import TYPE_CHECKING, Generic, Protocol, TypeVar, cast

from helpy._interfaces.settings_holder import UniqueSettingsHolder

from beekeepy._interface.abc.asynchronous.beekeeper import (
    Beekeeper as AsynchronousBeekeeperInterface,
)
from beekeepy._interface.abc.synchronous.beekeeper import (
    Beekeeper as SynchronousBeekeeperInterface,
)
from beekeepy._interface.settings import Settings

if TYPE_CHECKING:
    from helpy import HttpUrl
    from helpy import Settings as HelpySettings


T_co = TypeVar("T_co", bound=SynchronousBeekeeperInterface | AsynchronousBeekeeperInterface, covariant=True)


class _RemoteFactoryCallable(Protocol[T_co]):
    def __call__(self, *, url_or_settings: HttpUrl | Settings) -> T_co: ...


class Packed(UniqueSettingsHolder[Settings], Generic[T_co]):
    """Allows to transfer beekeeper handle to other process."""

    def __init__(self, settings: HelpySettings, unpack_factory: _RemoteFactoryCallable[T_co]) -> None:
        super().__init__(settings=cast(settings, Settings))
        self.__unpack_factory = unpack_factory
        self._prepare_settings_for_packing()

    def unpack(self) -> T_co:
        return self.__unpack_factory(url_or_settings=self.__settings)

    def _prepare_settings_for_packing(self) -> None:
        with self.update_settings() as settings:
            settings.notification_endpoint = None
