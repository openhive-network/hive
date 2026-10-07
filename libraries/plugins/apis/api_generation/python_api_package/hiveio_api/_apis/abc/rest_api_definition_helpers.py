# ruff: noqa: SLF001
# APIs call the protected interface of their owner (handle implementing Sendable) - by design.
from __future__ import annotations

from functools import wraps
from typing import TYPE_CHECKING, Callable, ParamSpec, TypeVar, get_type_hints

if TYPE_CHECKING:
    from hiveio_api._apis.abc.api import AbstractAsyncApi, AbstractSyncApi
    from hiveio_api._communication.abc.communicator_models import Methods

P = ParamSpec("P")
R = TypeVar("R")


class SyncRestApiDefinitionHelper:
    @classmethod
    def __define_impl_for_rest_method(
        cls, wrapped_function: Callable[P, R], method: Methods, path: str
    ) -> Callable[P, R]:
        """Decorator for REST methods in child classes."""
        type_hints = get_type_hints(wrapped_function)
        expected_type = type_hints["return"]

        @wraps(wrapped_function)
        def _impl(this: AbstractSyncApi, *args: P.args, **kwargs: P.kwargs) -> R:
            return this._owner._send(  # type: ignore[no-any-return]
                method=method,
                expected_type=expected_type,
                serialization_type=this._serialize_type(),
                data=None,
                url=this.prepare_rest_url(
                    positional_args=list(args), query_args=dict(kwargs), api_type_hints=type_hints, api_orig_path=path
                ),
            ).result

        return _impl  # type: ignore[return-value]

    @classmethod
    def get(cls, path: str) -> Callable[[Callable[P, R]], Callable[P, R]]:
        """Decorator for GET methods in child classes."""

        def _wrapper(wrapped_function: Callable[P, R]) -> Callable[P, R]:
            return cls.__define_impl_for_rest_method(wrapped_function=wrapped_function, method="GET", path=path)

        return _wrapper

    @classmethod
    def post(cls, path: str) -> Callable[[Callable[P, R]], Callable[P, R]]:
        """Decorator for POST methods in child classes."""

        def _wrapper(wrapped_function: Callable[P, R]) -> Callable[P, R]:
            return cls.__define_impl_for_rest_method(wrapped_function=wrapped_function, method="POST", path=path)

        return _wrapper

    @classmethod
    def put(cls, path: str) -> Callable[[Callable[P, R]], Callable[P, R]]:
        """Decorator for PUT methods in child classes."""

        def _wrapper(wrapped_function: Callable[P, R]) -> Callable[P, R]:
            return cls.__define_impl_for_rest_method(wrapped_function=wrapped_function, method="PUT", path=path)

        return _wrapper


class AsyncRestApiDefinitionHelper:
    @classmethod
    def __define_impl_for_rest_method(
        cls, wrapped_function: Callable[P, R], method: Methods, path: str
    ) -> Callable[P, R]:
        """Decorator for REST methods in child classes."""
        type_hints = get_type_hints(wrapped_function)
        expected_type = type_hints["return"]

        @wraps(wrapped_function)
        async def _impl(this: AbstractAsyncApi, *args: P.args, **kwargs: P.kwargs) -> R:
            return (  # type: ignore[no-any-return]
                await this._owner._async_send(
                    method=method,
                    expected_type=expected_type,
                    serialization_type=this._serialize_type(),
                    data=None,
                    url=this.prepare_rest_url(
                        positional_args=list(args),
                        query_args=dict(kwargs),
                        api_type_hints=type_hints,
                        api_orig_path=path,
                    ),
                )
            ).result

        return _impl  # type: ignore[return-value]

    @classmethod
    def get(cls, path: str) -> Callable[[Callable[P, R]], Callable[P, R]]:
        """Decorator for GET methods in child classes."""

        def _wrapper(wrapped_function: Callable[P, R]) -> Callable[P, R]:
            return cls.__define_impl_for_rest_method(wrapped_function=wrapped_function, method="GET", path=path)

        return _wrapper

    @classmethod
    def post(cls, path: str) -> Callable[[Callable[P, R]], Callable[P, R]]:
        """Decorator for POST methods in child classes."""

        def _wrapper(wrapped_function: Callable[P, R]) -> Callable[P, R]:
            return cls.__define_impl_for_rest_method(wrapped_function=wrapped_function, method="POST", path=path)

        return _wrapper

    @classmethod
    def put(cls, path: str) -> Callable[[Callable[P, R]], Callable[P, R]]:
        """Decorator for PUT methods in child classes."""

        def _wrapper(wrapped_function: Callable[P, R]) -> Callable[P, R]:
            return cls.__define_impl_for_rest_method(wrapped_function=wrapped_function, method="PUT", path=path)

        return _wrapper
