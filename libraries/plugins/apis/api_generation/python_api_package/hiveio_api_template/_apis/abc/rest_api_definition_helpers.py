# ruff: noqa: SLF001
# APIs call the protected interface of their owner (handle implementing Sendable) - by design.
from __future__ import annotations

from functools import wraps
from typing import TYPE_CHECKING, Callable, Concatenate, ParamSpec, TypeVar, get_type_hints

from hiveio_api._apis.abc.endpoint import Endpoint

if TYPE_CHECKING:
    from collections.abc import Awaitable

    from hiveio_api._apis.abc.api import AbstractAsyncApi, AbstractSyncApi
    from hiveio_api._communication.abc.communicator_models import AsyncCallbacks, Callbacks, Methods

P = ParamSpec("P")
R = TypeVar("R")
SyncApiT = TypeVar("SyncApiT", bound="AbstractSyncApi")
AsyncApiT = TypeVar("AsyncApiT", bound="AbstractAsyncApi")


class SyncRestApiDefinitionHelper:
    @classmethod
    def __define_impl_for_rest_method(
        cls, wrapped_function: Callable[Concatenate[SyncApiT, P], R], method: Methods, path: str
    ) -> Endpoint[SyncApiT, P, R, Callbacks]:
        """Decorator for REST methods in child classes."""
        type_hints = get_type_hints(wrapped_function)
        expected_type = type_hints["return"]

        @wraps(wrapped_function)
        def _impl(this: SyncApiT, /, *args: P.args, **kwargs: P.kwargs) -> R:
            return this._send_request(  # type: ignore[no-any-return]
                wrapped_function.__name__,
                method=method,
                expected_type=expected_type,
                url=this.prepare_rest_url(
                    positional_args=list(args), query_args=dict(kwargs), api_type_hints=type_hints, api_orig_path=path
                ),
            )

        return Endpoint(_impl, lambda api, endpoint_name: api._endpoint_callbacks(endpoint_name))

    @classmethod
    def get(cls, path: str) -> Callable[[Callable[Concatenate[SyncApiT, P], R]], Endpoint[SyncApiT, P, R, Callbacks]]:
        """Decorator for GET methods in child classes."""

        def _wrapper(wrapped_function: Callable[Concatenate[SyncApiT, P], R]) -> Endpoint[SyncApiT, P, R, Callbacks]:
            return cls.__define_impl_for_rest_method(wrapped_function=wrapped_function, method="GET", path=path)

        return _wrapper

    @classmethod
    def post(cls, path: str) -> Callable[[Callable[Concatenate[SyncApiT, P], R]], Endpoint[SyncApiT, P, R, Callbacks]]:
        """Decorator for POST methods in child classes."""

        def _wrapper(wrapped_function: Callable[Concatenate[SyncApiT, P], R]) -> Endpoint[SyncApiT, P, R, Callbacks]:
            return cls.__define_impl_for_rest_method(wrapped_function=wrapped_function, method="POST", path=path)

        return _wrapper

    @classmethod
    def put(cls, path: str) -> Callable[[Callable[Concatenate[SyncApiT, P], R]], Endpoint[SyncApiT, P, R, Callbacks]]:
        """Decorator for PUT methods in child classes."""

        def _wrapper(wrapped_function: Callable[Concatenate[SyncApiT, P], R]) -> Endpoint[SyncApiT, P, R, Callbacks]:
            return cls.__define_impl_for_rest_method(wrapped_function=wrapped_function, method="PUT", path=path)

        return _wrapper


class AsyncRestApiDefinitionHelper:
    @classmethod
    def __define_impl_for_rest_method(
        cls, wrapped_function: Callable[Concatenate[AsyncApiT, P], Awaitable[R]], method: Methods, path: str
    ) -> Endpoint[AsyncApiT, P, Awaitable[R], AsyncCallbacks]:
        """Decorator for REST methods in child classes."""
        type_hints = get_type_hints(wrapped_function)
        expected_type = type_hints["return"]

        @wraps(wrapped_function)
        async def _impl(this: AsyncApiT, /, *args: P.args, **kwargs: P.kwargs) -> R:
            return await this._async_send_request(
                wrapped_function.__name__,
                method=method,
                expected_type=expected_type,
                url=this.prepare_rest_url(
                    positional_args=list(args),
                    query_args=dict(kwargs),
                    api_type_hints=type_hints,
                    api_orig_path=path,
                ),
            )

        return Endpoint(_impl, lambda api, endpoint_name: api._endpoint_callbacks(endpoint_name))

    @classmethod
    def get(
        cls, path: str
    ) -> Callable[
        [Callable[Concatenate[AsyncApiT, P], Awaitable[R]]], Endpoint[AsyncApiT, P, Awaitable[R], AsyncCallbacks]
    ]:
        """Decorator for GET methods in child classes."""

        def _wrapper(
            wrapped_function: Callable[Concatenate[AsyncApiT, P], Awaitable[R]],
        ) -> Endpoint[AsyncApiT, P, Awaitable[R], AsyncCallbacks]:
            return cls.__define_impl_for_rest_method(wrapped_function=wrapped_function, method="GET", path=path)

        return _wrapper

    @classmethod
    def post(
        cls, path: str
    ) -> Callable[
        [Callable[Concatenate[AsyncApiT, P], Awaitable[R]]], Endpoint[AsyncApiT, P, Awaitable[R], AsyncCallbacks]
    ]:
        """Decorator for POST methods in child classes."""

        def _wrapper(
            wrapped_function: Callable[Concatenate[AsyncApiT, P], Awaitable[R]],
        ) -> Endpoint[AsyncApiT, P, Awaitable[R], AsyncCallbacks]:
            return cls.__define_impl_for_rest_method(wrapped_function=wrapped_function, method="POST", path=path)

        return _wrapper

    @classmethod
    def put(
        cls, path: str
    ) -> Callable[
        [Callable[Concatenate[AsyncApiT, P], Awaitable[R]]], Endpoint[AsyncApiT, P, Awaitable[R], AsyncCallbacks]
    ]:
        """Decorator for PUT methods in child classes."""

        def _wrapper(
            wrapped_function: Callable[Concatenate[AsyncApiT, P], Awaitable[R]],
        ) -> Endpoint[AsyncApiT, P, Awaitable[R], AsyncCallbacks]:
            return cls.__define_impl_for_rest_method(wrapped_function=wrapped_function, method="PUT", path=path)

        return _wrapper
