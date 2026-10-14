# ruff: noqa: SLF001
# APIs call the protected interface of their owner (handle implementing Sendable) - by design.
from __future__ import annotations

import re
from abc import ABC
from collections import defaultdict
from enum import IntEnum
from functools import wraps
from typing import (
    TYPE_CHECKING,
    ClassVar,
    Generic,
    ParamSpec,
    TypeVar,
    get_type_hints,
)

from hiveio_api._apis.abc.rest_api_definition_helpers import AsyncRestApiDefinitionHelper, SyncRestApiDefinitionHelper
from hiveio_api._apis.abc.sendable import AsyncSendable, SyncSendable
from hiveio_api._base import HiveModel, to_builtins
from hiveio_api._communication.url import HttpUrl
from hiveio_api._utilities.build_json_rpc_call import build_json_rpc_call

if TYPE_CHECKING:
    from collections.abc import Awaitable, Callable

P = ParamSpec("P")
R = TypeVar("R")
HandleT = TypeVar("HandleT", bound=SyncSendable | AsyncSendable)

RegisteredApisT = defaultdict[bool, defaultdict[str, set[str]]]
ApiArgumentsToSerialize = tuple[tuple[object, ...], dict[str, object]]


class ApiArgumentSerialization(IntEnum):
    OBJECT = 0
    ARRAY = 1
    DOUBLE_ARRAY = 2


def _convert_pascal_case_to_sneak_case(pascal_case_input: str) -> str:
    return re.sub(r"(?<!^)(?=[A-Z])", "_", pascal_case_input).lower()


def _public_models_to_builtins(value: object) -> object:
    """
    Convert public models in the params to builtins with keys of the API.

    msgspec encodes dataclasses by attribute names, which differ from the JSON keys for renamed fields
    (e.g. `from_` -> `from`), and would also emit `null` for fields missing in the data the model was built from.
    """
    if isinstance(value, HiveModel):
        return to_builtins(value)
    if type(value) is list or type(value) is tuple:
        return [_public_models_to_builtins(item) for item in value]
    if type(value) is dict:
        return {key: _public_models_to_builtins(item) for key, item in value.items()}
    return value


class AbstractApi(ABC, Generic[HandleT]):
    """Base class for apis."""

    __registered_apis: ClassVar[RegisteredApisT] = defaultdict(lambda: defaultdict(lambda: set()))

    @staticmethod
    def _get_api_name_from_method(method: Callable[P, R] | Callable[P, Awaitable[R]]) -> str:
        """Converts __qualname__ to api name."""
        return _convert_pascal_case_to_sneak_case(method.__qualname__.split(".")[0])

    def json_dumps(self) -> Callable[[object], str]:
        # Hive types (`schemas.fields`) are serialized in the form given by their type (e.g. `LegacyAssetHive`)
        from schemas.encoders import get_hf26_encoder

        encoder = get_hf26_encoder()
        return lambda x: encoder.encode(_public_models_to_builtins(x)).decode()

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

    def _additional_arguments_actions(
        self,
        endpoint_name: str,
        arguments: ApiArgumentsToSerialize,
    ) -> ApiArgumentsToSerialize:
        return self._prepare_arguments_for_serialization(arguments)

    @classmethod
    def endpoint_jsonrpc(cls, wrapped_function: Callable[P, R]) -> Callable[P, R]:
        """Decorator for all api methods in child classes."""
        wrapped_function_name = wrapped_function.__name__
        api_name = cls._get_api_name_from_method(wrapped_function)
        cls._register_method(api=api_name, endpoint=wrapped_function_name, sync=True)

        @wraps(wrapped_function)
        def impl(this: AbstractSyncApi, *args: P.args, **kwargs: P.kwargs) -> R:
            this._verify_positional_keyword_args(args, kwargs)
            endpoint = f"{api_name}.{wrapped_function_name}"
            args_, kwargs_ = this._additional_arguments_actions(endpoint, (args, kwargs))
            data = build_json_rpc_call(
                method=endpoint,
                params=this._serialize_params((args_, kwargs_)),
                id_=this._owner._id_for_jsonrpc_request(),
            )
            return this._owner._send(  # type: ignore[no-any-return]
                method="POST",
                expected_type=get_type_hints(wrapped_function)["return"],
                data=data,
            ).result

        return impl  # type: ignore[return-value]

    @classmethod
    def endpoint_rest(cls) -> type[SyncRestApiDefinitionHelper]:
        return SyncRestApiDefinitionHelper


class AbstractAsyncApi(AbstractApi[AsyncSendable]):
    """Base class for all apis, that provides asynchronous endpoints."""

    async def _additional_arguments_actions(
        self,
        endpoint_name: str,
        arguments: ApiArgumentsToSerialize,
    ) -> ApiArgumentsToSerialize:
        return self._prepare_arguments_for_serialization(arguments)

    @classmethod
    def endpoint_jsonrpc(cls, wrapped_function: Callable[P, Awaitable[R]]) -> Callable[P, Awaitable[R]]:
        """Decorator for all api methods in child classes."""
        wrapped_function_name = wrapped_function.__name__
        api_name = cls._get_api_name_from_method(wrapped_function)  # type: ignore[arg-type]
        cls._register_method(api=api_name, endpoint=wrapped_function_name, sync=False)

        @wraps(wrapped_function)
        async def impl(this: AbstractAsyncApi, *args: P.args, **kwargs: P.kwargs) -> R:
            this._verify_positional_keyword_args(args, kwargs)
            endpoint = f"{api_name}.{wrapped_function_name}"
            args_, kwargs_ = await this._additional_arguments_actions(endpoint, (args, kwargs))
            data = build_json_rpc_call(
                method=endpoint,
                params=this._serialize_params((args_, kwargs_)),
                id_=this._owner._id_for_jsonrpc_request(),
            )
            return (  # type: ignore[no-any-return]
                await this._owner._async_send(
                    method="POST",
                    expected_type=get_type_hints(wrapped_function)["return"],
                    data=data,
                )
            ).result

        return impl  # type: ignore[return-value]

    @classmethod
    def endpoint_rest(cls) -> type[AsyncRestApiDefinitionHelper]:
        return AsyncRestApiDefinitionHelper
