# ruff: noqa: SLF001
# APIs call the protected interface of their owner (handle implementing Sendable) - by design.
from __future__ import annotations

import json
import re
from abc import ABC
from collections import defaultdict
from enum import IntEnum
from functools import wraps
from typing import (
    TYPE_CHECKING,
    ClassVar,
    Concatenate,
    Generic,
    ParamSpec,
    TypeVar,
    get_type_hints,
)

from hiveio_api._apis.abc.endpoint import Endpoint
from hiveio_api._apis.abc.rest_api_definition_helpers import AsyncRestApiDefinitionHelper, SyncRestApiDefinitionHelper
from hiveio_api._apis.abc.sendable import AsyncSendable, SyncSendable
from hiveio_api._base import to_api_builtins
from hiveio_api._communication.abc.communicator_models import AsyncCallbacks, Callbacks
from hiveio_api._communication.url import HttpUrl
from hiveio_api._utilities.build_json_rpc_call import build_json_rpc_call

if TYPE_CHECKING:
    from collections.abc import Awaitable, Callable

    from hiveio_api._apis.abc.api_collection import AbstractAsyncApiCollection, AbstractSyncApiCollection
    from hiveio_api._communication.abc.communicator_models import Methods
    from hiveio_api._communication.jsonrpc import ExpectResultT

P = ParamSpec("P")
R = TypeVar("R")
HandleT = TypeVar("HandleT", bound=SyncSendable | AsyncSendable)
SyncApiT = TypeVar("SyncApiT", bound="AbstractSyncApi")
AsyncApiT = TypeVar("AsyncApiT", bound="AbstractAsyncApi")

RegisteredApisT = defaultdict[bool, defaultdict[str, set[str]]]
ApiArgumentsToSerialize = tuple[tuple[object, ...], dict[str, object]]


def _validating_type_to_builtins(value: object) -> object:
    """Builtins of values `json` can not serialize - validating Hive types of `schemas.fields` (msgspec models)."""
    import msgspec  # validation machinery - imported only when such a type is sent
    from schemas.encoders import enc_hook_hf26

    return msgspec.to_builtins(value, enc_hook=enc_hook_hf26)


class ApiArgumentSerialization(IntEnum):
    OBJECT = 0
    ARRAY = 1
    DOUBLE_ARRAY = 2


def _convert_pascal_case_to_sneak_case(pascal_case_input: str) -> str:
    return re.sub(r"(?<!^)(?=[A-Z])", "_", pascal_case_input).lower()


class _ApiCallbacks[CallbacksT: (Callbacks, AsyncCallbacks)]:
    """Callbacks of an API (`root`) and of its endpoints, joined with the callbacks of its collection for a request."""

    def __init__(self, callbacks_type: type[CallbacksT]) -> None:
        self.__type: type[CallbacksT] = callbacks_type
        self.root: CallbacksT = callbacks_type()
        self.collection: CallbacksT | None = None
        """Callbacks of the collection the API is an attribute of."""
        self.__endpoints: dict[str, CallbacksT] = {}

    def of_endpoint(self, endpoint_name: str) -> CallbacksT:
        return self.__endpoints.setdefault(endpoint_name, self.__type())

    def of_request(self, endpoint_name: str) -> CallbacksT:
        """Callbacks of a request of the endpoint - of the collection, of the API and of the endpoint, in this order."""
        parts = (self.collection, self.root, self.__endpoints.get(endpoint_name))
        return self.__type.joined(*(part for part in parts if part is not None))


class AbstractApi(ABC, Generic[HandleT]):
    """Base class for apis."""

    __registered_apis: ClassVar[RegisteredApisT] = defaultdict(lambda: defaultdict(lambda: set()))

    @staticmethod
    def _get_api_name_from_method(method: Callable[..., object]) -> str:
        """Converts __qualname__ to api name."""
        return _convert_pascal_case_to_sneak_case(method.__qualname__.split(".")[0])

    def json_dumps(self) -> Callable[[object], str]:
        """
        Serializer of parameters - public models with keys of the API, assets in the form of their fields.

        Assets in fields of legacy assets are sent with symbols of the network of the owner (`is_testnet` is asked only
        for them - it may send a request itself). Validating Hive types (`schemas.fields`) are serialized in the form
        given by their type (e.g. `LegacyAssetHive`).
        """
        is_testnet = self._owner.is_testnet
        return lambda x: json.dumps(
            to_api_builtins(x, testnet=is_testnet), default=_validating_type_to_builtins, separators=(",", ":"),
            ensure_ascii=False,
        )

    def _serialize_params(self, arguments: ApiArgumentsToSerialize) -> str:
        """Return serialized given params. Can be overloaded."""
        json_dumps = self.json_dumps()
        if self.argument_serialization() == ApiArgumentSerialization.ARRAY:
            return json_dumps(arguments[0])
        if self.argument_serialization() == ApiArgumentSerialization.DOUBLE_ARRAY:
            return json_dumps([arguments[0]])
        prepared_kwargs = {}
        for key, value in arguments[1].items():
            prepared_kwargs[key.strip("_")] = value
        return json_dumps(prepared_kwargs)

    def _serialize_single_param(self, param: object) -> str:
        """Return serialized single param. Can be overloaded."""
        return self.json_dumps()(param).strip('"')

    def _verify_positional_keyword_args(self, args: tuple[object, ...], kwargs: dict[str, object]) -> None:
        if self.argument_serialization() == ApiArgumentSerialization.OBJECT:
            assert len(args) == 0, "This api allows only keyword arguments; Ex.: foo(a=1, b=2, c=3)"
        else:
            assert len(kwargs) == 0, "This api allows only positional arguments; Ex.: foo(1, 2, 3)"

    @classmethod
    def _api_name(cls) -> str:
        """Return api name. By default uses class name. Can be overloaded."""
        return _convert_pascal_case_to_sneak_case(cls.__name__)

    @classmethod
    def _register_api(cls) -> bool:
        """Defines is apis should be registered in global collection."""
        return True

    @classmethod
    def _register_method(cls, *, api: str, endpoint: str, sync: bool) -> None:
        """For tests purposes only; Registers apis in global collection."""
        if cls._register_api():
            cls.__registered_apis[sync][api].add(endpoint)

    @classmethod
    def _get_registered_methods(cls) -> RegisteredApisT:
        """Returns registered endpoints."""
        return cls.__registered_apis.copy()

    def _prepare_arguments_for_serialization(self, arguments: ApiArgumentsToSerialize) -> ApiArgumentsToSerialize:
        return arguments

    def argument_serialization(self) -> ApiArgumentSerialization:
        return ApiArgumentSerialization.OBJECT

    def base_path(self) -> str:
        """Returns base path for api."""
        return ""

    def prepare_rest_url(
        self,
        positional_args: list[object],
        query_args: dict[str, object],
        api_type_hints: dict[str, object],
        api_orig_path: str,
    ) -> HttpUrl:
        # Prepare path in url
        type_hint_names = list(api_type_hints.keys())
        arguments_to_format: dict[str, str] = {}
        for i, arg in enumerate(positional_args):
            arguments_to_format[type_hint_names[i].replace("_", "-")] = self._serialize_single_param(arg)
        path = (self.base_path() + api_orig_path).format(**arguments_to_format)

        return HttpUrl.factory(
            path=path, query={k.replace("_", "-"): self._serialize_single_param(v) for k, v in query_args.items()}
        )

    def __init__(self, owner: HandleT) -> None:
        self._owner = owner


class AbstractSyncApi(AbstractApi[SyncSendable]):
    """Base class for all apis, that provides synchronous endpoints."""

    def __init__(self, owner: SyncSendable) -> None:
        super().__init__(owner)
        self.__callbacks = _ApiCallbacks(Callbacks)

    @property
    def callbacks(self) -> Callbacks:
        """Callbacks of requests of all endpoints of the API - called after callbacks of the API collection."""
        return self.__callbacks.root

    def _endpoint_callbacks(self, endpoint_name: str) -> Callbacks:
        return self.__callbacks.of_endpoint(endpoint_name)

    def _attach_to_collection(self, collection: AbstractSyncApiCollection) -> None:
        self.__callbacks.collection = collection.callbacks

    def _send_request(
        self,
        endpoint_name: str,
        *,
        method: Methods,
        expected_type: type[ExpectResultT],
        data: str | None = None,
        url: HttpUrl | None = None,
    ) -> ExpectResultT:
        """Send the request of the endpoint through the owner, with callbacks of the collection, API and endpoint."""
        callbacks = self.__callbacks.of_request(endpoint_name)
        return self._owner._send(
            method=method, expected_type=expected_type, data=data, url=url, callbacks=callbacks
        ).result

    def _additional_arguments_actions(
        self,
        endpoint_name: str,
        arguments: ApiArgumentsToSerialize,
    ) -> ApiArgumentsToSerialize:
        return self._prepare_arguments_for_serialization(arguments)

    @classmethod
    def endpoint_jsonrpc(
        cls, wrapped_function: Callable[Concatenate[SyncApiT, P], R]
    ) -> Endpoint[SyncApiT, P, R, Callbacks]:
        """Decorator for all api methods in child classes."""
        wrapped_function_name = wrapped_function.__name__
        api_name = cls._get_api_name_from_method(wrapped_function)
        cls._register_method(api=api_name, endpoint=wrapped_function_name, sync=True)

        @wraps(wrapped_function)
        def impl(this: SyncApiT, /, *args: P.args, **kwargs: P.kwargs) -> R:
            this._verify_positional_keyword_args(args, kwargs)
            endpoint = f"{api_name}.{wrapped_function_name}"
            args_, kwargs_ = this._additional_arguments_actions(endpoint, (args, kwargs))
            data = build_json_rpc_call(
                method=endpoint,
                params=this._serialize_params((args_, kwargs_)),
                id_=this._owner._id_for_jsonrpc_request(),
            )
            return this._send_request(  # type: ignore[no-any-return]
                wrapped_function_name,
                method="POST",
                expected_type=get_type_hints(wrapped_function)["return"],
                data=data,
            )

        return Endpoint(impl, AbstractSyncApi._endpoint_callbacks)

    @classmethod
    def endpoint_rest(cls) -> type[SyncRestApiDefinitionHelper]:
        return SyncRestApiDefinitionHelper


class AbstractAsyncApi(AbstractApi[AsyncSendable]):
    """Base class for all apis, that provides asynchronous endpoints."""

    def __init__(self, owner: AsyncSendable) -> None:
        super().__init__(owner)
        self.__callbacks = _ApiCallbacks(AsyncCallbacks)

    @property
    def callbacks(self) -> AsyncCallbacks:
        """Callbacks of requests of all endpoints of the API - called after callbacks of the API collection."""
        return self.__callbacks.root

    def _endpoint_callbacks(self, endpoint_name: str) -> AsyncCallbacks:
        return self.__callbacks.of_endpoint(endpoint_name)

    def _attach_to_collection(self, collection: AbstractAsyncApiCollection) -> None:
        self.__callbacks.collection = collection.callbacks

    async def _async_send_request(
        self,
        endpoint_name: str,
        *,
        method: Methods,
        expected_type: type[ExpectResultT],
        data: str | None = None,
        url: HttpUrl | None = None,
    ) -> ExpectResultT:
        """Send the request of the endpoint through the owner, with callbacks of the collection, API and endpoint."""
        callbacks = self.__callbacks.of_request(endpoint_name)
        return (
            await self._owner._async_send(
                method=method, expected_type=expected_type, data=data, url=url, callbacks=callbacks
            )
        ).result

    async def _additional_arguments_actions(
        self,
        endpoint_name: str,
        arguments: ApiArgumentsToSerialize,
    ) -> ApiArgumentsToSerialize:
        return self._prepare_arguments_for_serialization(arguments)

    @classmethod
    def endpoint_jsonrpc(
        cls, wrapped_function: Callable[Concatenate[AsyncApiT, P], Awaitable[R]]
    ) -> Endpoint[AsyncApiT, P, Awaitable[R], AsyncCallbacks]:
        """Decorator for all api methods in child classes."""
        wrapped_function_name = wrapped_function.__name__
        api_name = cls._get_api_name_from_method(wrapped_function)
        cls._register_method(api=api_name, endpoint=wrapped_function_name, sync=False)

        @wraps(wrapped_function)
        async def impl(this: AsyncApiT, /, *args: P.args, **kwargs: P.kwargs) -> R:
            this._verify_positional_keyword_args(args, kwargs)
            endpoint = f"{api_name}.{wrapped_function_name}"
            args_, kwargs_ = await this._additional_arguments_actions(endpoint, (args, kwargs))
            data = build_json_rpc_call(
                method=endpoint,
                params=this._serialize_params((args_, kwargs_)),
                id_=this._owner._id_for_jsonrpc_request(),
            )
            return await this._async_send_request(
                wrapped_function_name,
                method="POST",
                expected_type=get_type_hints(wrapped_function)["return"],
                data=data,
            )

        return Endpoint(impl, AbstractAsyncApi._endpoint_callbacks)

    @classmethod
    def endpoint_rest(cls) -> type[AsyncRestApiDefinitionHelper]:
        return AsyncRestApiDefinitionHelper
