from __future__ import annotations

from typing import TYPE_CHECKING, Any, Generic, Protocol, TypeVar, cast

if TYPE_CHECKING:
    from beekeepy.settings import Settings
    from helpy import HttpUrl

T = TypeVar("T")


class _RemoteFactoryCallable(Protocol):  # Generic[T]:
    # Protocools are quite limitted for generics:
    # https://stackoverflow.com/questions/61467673/how-do-i-create-a-generic-interface-in-python
    def __call__(self, *, url_or_settings: HttpUrl | Settings) -> Any:
        ...


class Packed(Generic[T]):
    def __init__(self, settings: Settings, unpack_factory: _RemoteFactoryCallable) -> None:
        self.__settings = settings
        self.__unpack_factory = unpack_factory

    def unpack(self) -> T:
        return cast(T, self.__unpack_factory(url_or_settings=self.__settings))
