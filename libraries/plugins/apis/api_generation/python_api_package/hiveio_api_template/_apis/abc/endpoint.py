from __future__ import annotations

from functools import update_wrapper
from typing import TYPE_CHECKING, Concatenate, Generic, ParamSpec, TypeVar, overload

if TYPE_CHECKING:
    from collections.abc import Callable
    from typing import Self

ApiT = TypeVar("ApiT")
P = ParamSpec("P")
R = TypeVar("R")
CallbacksT = TypeVar("CallbacksT")


class Endpoint(Generic[ApiT, P, R, CallbacksT]):
    """
    Endpoint of an API class (created by the endpoint decorators of APIs).

    Accessed through an instance of the API, it is bound to it - `api.endpoint(...)` calls the endpoint,
    `api.endpoint.callbacks` are callbacks of the endpoint in this instance of the API.
    """

    def __init__(
        self, function: Callable[Concatenate[ApiT, P], R], callbacks_of: Callable[[ApiT, str], CallbacksT]
    ) -> None:
        """
        Args:
            function: Implementation of the endpoint, called with the instance of the API.
            callbacks_of: Returns callbacks of the endpoint (given by name) in the given instance of the API.
        """
        update_wrapper(self, function)
        self.__function = function
        self.__callbacks_of = callbacks_of
        self.__name = function.__name__

    def __call__(self, api: ApiT, /, *args: P.args, **kwargs: P.kwargs) -> R:
        """Call the endpoint of the given instance of the API (as `ApiClass.endpoint(api, ...)`)."""
        return self.__function(api, *args, **kwargs)

    @overload
    def __get__(self, api: None, owner: type[object] | None = None) -> Self: ...

    @overload
    def __get__(self, api: ApiT, owner: type[object] | None = None) -> BoundEndpoint[ApiT, P, R, CallbacksT]: ...

    def __get__(
        self, api: ApiT | None, owner: type[object] | None = None
    ) -> Self | BoundEndpoint[ApiT, P, R, CallbacksT]:
        if api is None:
            return self
        bound = BoundEndpoint(api, self, self.__function, self.__callbacks_of)
        # kept in the instance - the next accesses get it without the descriptor (as `functools.cached_property`)
        vars(api)[self.__name] = bound
        return bound


class BoundEndpoint(Generic[ApiT, P, R, CallbacksT]):
    """Endpoint bound to an instance of its API - callable as the method, with callbacks of the endpoint."""

    def __init__(
        self,
        api: ApiT,
        endpoint: Endpoint[ApiT, P, R, CallbacksT],
        function: Callable[Concatenate[ApiT, P], R],
        callbacks_of: Callable[[ApiT, str], CallbacksT],
    ) -> None:
        update_wrapper(self, function, updated=())  # name, documentation, ... - not attributes of the function
        self.__self__ = api
        self.__func__ = endpoint
        self.__function = function
        self.__callbacks_of = callbacks_of
        self.__name = function.__name__

    def __call__(self, *args: P.args, **kwargs: P.kwargs) -> R:
        return self.__function(self.__self__, *args, **kwargs)

    @property
    def callbacks(self) -> CallbacksT:
        """Callbacks of requests of this endpoint - called after callbacks of the API collection and of the API."""
        return self.__callbacks_of(self.__self__, self.__name)
