from __future__ import annotations

from typing import TYPE_CHECKING

from beekeepy._factories.generic_factories import _beekeeper_factory_impl, _beekeeper_remote_factory_impl
from beekeepy._factories.helpers import _prepare_settings_for_packing
from beekeepy._handle.beekeeper import AsyncBeekeeper as AsynchronousBeekeeperHandle
from beekeepy._handle.beekeeper import AsyncRemoteBeekeeper as AsynchronousRemoteBeekeeperHandle
from beekeepy._handle.beekeeper import Beekeeper as SynchronousBeekeeperHandle
from beekeepy._handle.beekeeper import SyncRemoteBeekeeper as SynchronousRemoteBeekeeperHandle
from beekeepy._interface.abc.asynchronous.beekeeper import (
    Beekeeper as AsynchronousBeekeeperInterface,
)
from beekeepy._interface.abc.packed_object import Packed
from beekeepy._interface.abc.synchronous.beekeeper import (
    Beekeeper as SynchronousBeekeeperInterface,
)
from beekeepy._interface.asynchronous.beekeeper import (
    Beekeeper as AsynchronousBeekeeper,
)
from beekeepy._interface.synchronous.beekeeper import Beekeeper as SynchronousBeekeeper

if TYPE_CHECKING:
    from beekeepy._interface.settings import Settings
    from helpy import HttpUrl

PackedBeekeeper = Packed[SynchronousBeekeeperInterface]
PackedAsyncBeekeeper = Packed[AsynchronousBeekeeperInterface]


class _SynchronousBeekeeperImpl(SynchronousBeekeeper):
    def pack(self) -> PackedBeekeeper:
        return Packed(
            settings=_prepare_settings_for_packing(self._get_instance().settings),
            unpack_factory=beekeeper_remote_factory,
        )


class _AsynchronousBeekeeperImpl(AsynchronousBeekeeper):
    def pack(self) -> PackedAsyncBeekeeper:
        return Packed(
            settings=_prepare_settings_for_packing(self._get_instance().settings),
            unpack_factory=async_beekeeper_remote_factory,
        )


def beekeeper_factory(*, settings: Settings | None = None) -> SynchronousBeekeeperInterface:
    """Creates an beekeeper object, which allows to manage keys and sign with them.

    Note:
        Remember to call .delete() if you don't use with statement

    Keyword Arguments:
        settings: Adjustable parameters of connection and workspace (default: {None})
    """
    return _beekeeper_factory_impl(_SynchronousBeekeeperImpl, SynchronousBeekeeperHandle, beekeeper_remote_factory, settings)  # type: ignore[return-value]


def beekeeper_remote_factory(*, url_or_settings: HttpUrl | Settings) -> SynchronousBeekeeperInterface:
    """Same as beekeeper_factory, but allows quickly to connect to remote beekeeper instance."""
    return _beekeeper_remote_factory_impl(_SynchronousBeekeeperImpl, SynchronousRemoteBeekeeperHandle, url_or_settings)  # type: ignore[return-value]


def async_beekeeper_factory(*, settings: Settings | None = None) -> AsynchronousBeekeeperInterface:
    """Same as beekeeper_factory but in async version."""
    return _beekeeper_factory_impl(_AsynchronousBeekeeperImpl, AsynchronousBeekeeperHandle, async_beekeeper_remote_factory, settings)  # type: ignore[return-value]


def async_beekeeper_remote_factory(*, url_or_settings: HttpUrl | Settings) -> AsynchronousBeekeeperInterface:
    """Same as beekeeper_remote_factory but in async version."""
    return _beekeeper_remote_factory_impl(_AsynchronousBeekeeperImpl, AsynchronousRemoteBeekeeperHandle, url_or_settings)  # type: ignore[return-value]
